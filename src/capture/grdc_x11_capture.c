#include "capture/grdc_x11_capture.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/XShm.h>

#include <gio/gio.h>
#include <glib.h>

#include <errno.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "utils/grdc_frame.h"
#include "utils/grdc_log.h"

typedef struct
{
    XShmSegmentInfo info;
} GrdcX11ShmArea;

struct _GrdcX11Capture
{
    GObject parent_instance;

    GMutex state_mutex;
    GrdcFrameQueue *queue;
    GThread *thread;

    gboolean running;
    gchar *display_name;

    Display *display;
    int screen;
    Window root;
    XImage *image;
    GrdcX11ShmArea shm;
    gboolean attached;
    Damage damage;
    int damage_event_base;

    guint width;
    guint height;
};

G_DEFINE_TYPE(GrdcX11Capture, grdc_x11_capture, G_TYPE_OBJECT)

static gpointer grdc_x11_capture_thread(gpointer user_data);
static void grdc_x11_capture_cleanup_locked(GrdcX11Capture *self);

static void
grdc_x11_capture_dispose(GObject *object)
{
    GrdcX11Capture *self = GRDC_X11_CAPTURE(object);

    grdc_x11_capture_stop(self);

    g_clear_pointer(&self->display_name, g_free);
    g_clear_object(&self->queue);

    G_OBJECT_CLASS(grdc_x11_capture_parent_class)->dispose(object);
}

static void
grdc_x11_capture_finalize(GObject *object)
{
    GrdcX11Capture *self = GRDC_X11_CAPTURE(object);
    g_mutex_clear(&self->state_mutex);
    G_OBJECT_CLASS(grdc_x11_capture_parent_class)->finalize(object);
}

static void
grdc_x11_capture_class_init(GrdcX11CaptureClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = grdc_x11_capture_dispose;
    object_class->finalize = grdc_x11_capture_finalize;
}

static void
grdc_x11_capture_init(GrdcX11Capture *self)
{
    g_mutex_init(&self->state_mutex);
    memset(&self->shm.info, 0, sizeof(self->shm.info));
    self->shm.info.shmid = -1;
    self->running = FALSE;
}

GrdcX11Capture *
grdc_x11_capture_new(GrdcFrameQueue *queue)
{
    g_return_val_if_fail(GRDC_IS_FRAME_QUEUE(queue), NULL);

    GrdcX11Capture *self = g_object_new(GRDC_TYPE_X11_CAPTURE, NULL);
    self->queue = g_object_ref(queue);
    return self;
}

static gboolean
grdc_x11_capture_prepare_display(GrdcX11Capture *self,
                                 const gchar *display_name,
                                 guint requested_width,
                                 guint requested_height,
                                 GError **error)
{
    self->display = XOpenDisplay(display_name);
    if (self->display == NULL)
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "Failed to open X11 display");
        return FALSE;
    }

    if (!XShmQueryExtension(self->display))
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_NOT_SUPPORTED,
                            "XShm extension not available on X server");
        return FALSE;
    }

    int damage_event = 0;
    int damage_error = 0;
    if (!XDamageQueryExtension(self->display, &damage_event, &damage_error))
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_NOT_SUPPORTED,
                            "XDamage extension not available on X server");
        return FALSE;
    }
    self->damage_event_base = damage_event;

    self->screen = DefaultScreen(self->display);
    self->root = RootWindow(self->display, self->screen);

    self->width = (requested_width > 0) ? requested_width : (guint)DisplayWidth(self->display, self->screen);
    self->height = (requested_height > 0) ? requested_height : (guint)DisplayHeight(self->display, self->screen);

    self->image = XShmCreateImage(self->display,
                                  DefaultVisual(self->display, self->screen),
                                  DefaultDepth(self->display, self->screen),
                                  ZPixmap,
                                  NULL,
                                  &self->shm.info,
                                  (int)self->width,
                                  (int)self->height);
    if (self->image == NULL)
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "Failed to create XShm image");
        return FALSE;
    }

    const size_t image_size = (size_t)self->image->bytes_per_line * (size_t)self->image->height;
    self->shm.info.shmid = shmget(IPC_PRIVATE, image_size, IPC_CREAT | 0600);
    if (self->shm.info.shmid < 0)
    {
        g_set_error(error,
                    G_IO_ERROR,
                    g_io_error_from_errno(errno),
                    "shmget failed: %s",
                    g_strerror(errno));
        return FALSE;
    }

    self->shm.info.shmaddr = (char *)shmat(self->shm.info.shmid, NULL, 0);
    if (self->shm.info.shmaddr == (char *)(-1))
    {
        g_set_error(error,
                    G_IO_ERROR,
                    g_io_error_from_errno(errno),
                    "shmat failed: %s",
                    g_strerror(errno));
        return FALSE;
    }

    self->shm.info.readOnly = False;
    self->image->data = self->shm.info.shmaddr;

    if (!XShmAttach(self->display, &self->shm.info))
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "XShmAttach failed");
        return FALSE;
    }
    self->attached = TRUE;

    self->damage = XDamageCreate(self->display, self->root, XDamageReportNonEmpty);
    if (self->damage == 0)
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "Failed to create XDamage handle");
        return FALSE;
    }

    XSync(self->display, False);
    return TRUE;
}

gboolean
grdc_x11_capture_start(GrdcX11Capture *self,
                        const gchar *display_name,
                        guint requested_width,
                        guint requested_height,
                        GError **error)
{
    g_return_val_if_fail(GRDC_IS_X11_CAPTURE(self), FALSE);

    g_mutex_lock(&self->state_mutex);
    if (self->running)
    {
        g_mutex_unlock(&self->state_mutex);
        return TRUE;
    }

    g_clear_pointer(&self->display_name, g_free);
    if (display_name != NULL)
    {
        self->display_name = g_strdup(display_name);
    }

    if (!grdc_x11_capture_prepare_display(self,
                                          self->display_name,
                                          requested_width,
                                          requested_height,
                                          error))
    {
        grdc_x11_capture_cleanup_locked(self);
        g_mutex_unlock(&self->state_mutex);
        return FALSE;
    }

    self->running = TRUE;
    self->thread = g_thread_new("grdc-x11-capture", grdc_x11_capture_thread, g_object_ref(self));

    g_mutex_unlock(&self->state_mutex);
    GRDC_LOG_MESSAGE("X11 capture started at %ux%u", self->width, self->height);
    return TRUE;
}

static void
grdc_x11_capture_cleanup_locked(GrdcX11Capture *self)
{
    if (self->damage != 0 && self->display != NULL)
    {
        XDamageDestroy(self->display, self->damage);
        self->damage = 0;
    }

    if (self->attached && self->display != NULL)
    {
        XShmDetach(self->display, &self->shm.info);
        self->attached = FALSE;
    }

    if (self->image != NULL)
    {
        self->image->data = NULL;
        XDestroyImage(self->image);
        self->image = NULL;
    }

    if (self->shm.info.shmaddr != NULL)
    {
        shmdt(self->shm.info.shmaddr);
        self->shm.info.shmaddr = NULL;
    }

    if (self->shm.info.shmid >= 0)
    {
        shmctl(self->shm.info.shmid, IPC_RMID, NULL);
        self->shm.info.shmid = -1;
        self->shm.info.shmseg = 0;
    }

    if (self->display != NULL)
    {
        XCloseDisplay(self->display);
        self->display = NULL;
    }
}

void
grdc_x11_capture_stop(GrdcX11Capture *self)
{
    g_return_if_fail(GRDC_IS_X11_CAPTURE(self));

    g_mutex_lock(&self->state_mutex);
    if (!self->running)
    {
        g_mutex_unlock(&self->state_mutex);
        return;
    }

    self->running = FALSE;
    Display *display = self->display;
    g_mutex_unlock(&self->state_mutex);

    if (display != NULL)
    {
        XSync(display, False);
    }

    if (self->thread != NULL)
    {
        g_thread_join(self->thread);
        self->thread = NULL;
    }

    g_mutex_lock(&self->state_mutex);
    grdc_x11_capture_cleanup_locked(self);
    g_mutex_unlock(&self->state_mutex);

    GRDC_LOG_MESSAGE("X11 capture stopped");
}

gboolean
grdc_x11_capture_is_running(GrdcX11Capture *self)
{
    g_return_val_if_fail(GRDC_IS_X11_CAPTURE(self), FALSE);

    g_mutex_lock(&self->state_mutex);
    gboolean running = self->running;
    g_mutex_unlock(&self->state_mutex);
    return running;
}

static gpointer
grdc_x11_capture_thread(gpointer user_data)
{
    GrdcX11Capture *self = GRDC_X11_CAPTURE(user_data);

    const gint64 target_interval = G_USEC_PER_SEC / 60;
    gint64 last_capture = 0;

    while (TRUE)
    {
        Display *display = NULL;
        XImage *image = NULL;
        Window root;
        int damage_event_base = 0;
        guint width = 0;
        guint height = 0;
        gboolean running;

        g_mutex_lock(&self->state_mutex);
        running = self->running;
        display = self->display;
        image = self->image;
        root = self->root;
        damage_event_base = self->damage_event_base;
        width = self->width;
        height = self->height;
        g_mutex_unlock(&self->state_mutex);

        if (!running || display == NULL || image == NULL)
        {
            break;
        }

        gboolean has_damage = FALSE;
        while (XPending(display) > 0)
        {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == damage_event_base + XDamageNotify)
            {
                has_damage = TRUE;
                XDamageSubtract(display, self->damage, None, None);
            }
        }

        gint64 now = g_get_monotonic_time();
        if (!has_damage && last_capture != 0 && (now - last_capture) < target_interval)
        {
            gint64 sleep_us = target_interval - (now - last_capture);
            if (sleep_us > 0)
            {
                if (sleep_us < 1000)
                {
                    sleep_us = 1000;
                }
                else if (sleep_us > target_interval)
                {
                    sleep_us = target_interval;
                }

                g_usleep((gulong)sleep_us);
            }
            continue;
        }

        if (!XShmGetImage(display, root, image, 0, 0, AllPlanes))
        {
            GRDC_LOG_WARNING("XShmGetImage failed, retrying");
            g_usleep((gulong)target_interval);
            continue;
        }

        g_autoptr(GrdcFrame) frame = grdc_frame_new();
        grdc_frame_configure(frame,
                             width,
                             height,
                             (guint)image->bytes_per_line,
                             (guint64)now);

        const gsize frame_size = (gsize)image->bytes_per_line * (gsize)image->height;
        guint8 *buffer = grdc_frame_ensure_capacity(frame, frame_size);
        if (buffer != NULL)
        {
            memcpy(buffer, image->data, frame_size);
            grdc_frame_queue_push(self->queue, frame);
        }

        last_capture = now;
    }

    g_object_unref(self);
    return NULL;
}
