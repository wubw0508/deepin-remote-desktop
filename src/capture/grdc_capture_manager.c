#include "capture/grdc_capture_manager.h"

#include <gio/gio.h>

#include "capture/grdc_x11_capture.h"
#include "utils/grdc_log.h"

struct _GrdcCaptureManager
{
    GObject parent_instance;
    gboolean running;
    GrdcFrameQueue *queue;
    GrdcX11Capture *x11_capture;
};

G_DEFINE_TYPE(GrdcCaptureManager, grdc_capture_manager, G_TYPE_OBJECT)

static void
grdc_capture_manager_dispose(GObject *object)
{
    GrdcCaptureManager *self = GRDC_CAPTURE_MANAGER(object);
    if (self->running)
    {
        grdc_capture_manager_stop(self);
    }

    g_clear_object(&self->queue);
    g_clear_object(&self->x11_capture);

    G_OBJECT_CLASS(grdc_capture_manager_parent_class)->dispose(object);
}

static void
grdc_capture_manager_class_init(GrdcCaptureManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = grdc_capture_manager_dispose;
}

static void
grdc_capture_manager_init(GrdcCaptureManager *self)
{
    self->running = FALSE;
    self->queue = grdc_frame_queue_new();
    self->x11_capture = grdc_x11_capture_new(self->queue);
}

GrdcCaptureManager *
grdc_capture_manager_new(void)
{
    return g_object_new(GRDC_TYPE_CAPTURE_MANAGER, NULL);
}

gboolean
grdc_capture_manager_start(GrdcCaptureManager *self, guint width, guint height, GError **error)
{
    g_return_val_if_fail(GRDC_IS_CAPTURE_MANAGER(self), FALSE);

    (void)error;

    if (self->running)
    {
        return TRUE;
    }

    grdc_frame_queue_reset(self->queue);

    if (!grdc_x11_capture_start(self->x11_capture, NULL, width, height, error))
    {
        grdc_frame_queue_stop(self->queue);
        return FALSE;
    }

    GRDC_LOG_MESSAGE("Capture manager entering running state");
    self->running = TRUE;
    return TRUE;
}

void
grdc_capture_manager_stop(GrdcCaptureManager *self)
{
    g_return_if_fail(GRDC_IS_CAPTURE_MANAGER(self));

    if (!self->running)
    {
        return;
    }

    grdc_x11_capture_stop(self->x11_capture);
    grdc_frame_queue_stop(self->queue);

    GRDC_LOG_MESSAGE("Capture manager leaving running state");
    self->running = FALSE;
}

gboolean
grdc_capture_manager_is_running(GrdcCaptureManager *self)
{
    g_return_val_if_fail(GRDC_IS_CAPTURE_MANAGER(self), FALSE);
    return self->running;
}

GrdcFrameQueue *
grdc_capture_manager_get_queue(GrdcCaptureManager *self)
{
    g_return_val_if_fail(GRDC_IS_CAPTURE_MANAGER(self), NULL);
    return self->queue;
}

gboolean
grdc_capture_manager_wait_frame(GrdcCaptureManager *self,
                                 gint64 timeout_us,
                                 GrdcFrame **out_frame,
                                 GError **error)
{
    g_return_val_if_fail(GRDC_IS_CAPTURE_MANAGER(self), FALSE);
    g_return_val_if_fail(out_frame != NULL, FALSE);

    if (!self->running)
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "Capture manager is not running");
        return FALSE;
    }

    if (!grdc_frame_queue_wait(self->queue, timeout_us, out_frame))
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_TIMED_OUT,
                            "Timed out waiting for capture frame");
        return FALSE;
    }

    return TRUE;
}
