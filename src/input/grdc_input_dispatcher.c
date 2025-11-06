#include "input/grdc_input_dispatcher.h"

#include <gio/gio.h>

#include "input/grdc_x11_input.h"

struct _GrdcInputDispatcher
{
    GObject parent_instance;

    GrdcX11Input *backend;
    gboolean active;
};

G_DEFINE_TYPE(GrdcInputDispatcher, grdc_input_dispatcher, G_TYPE_OBJECT)

static void
grdc_input_dispatcher_dispose(GObject *object)
{
    GrdcInputDispatcher *self = GRDC_INPUT_DISPATCHER(object);
    grdc_input_dispatcher_stop(self);
    g_clear_object(&self->backend);

    G_OBJECT_CLASS(grdc_input_dispatcher_parent_class)->dispose(object);
}

static void
grdc_input_dispatcher_class_init(GrdcInputDispatcherClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = grdc_input_dispatcher_dispose;
}

static void
grdc_input_dispatcher_init(GrdcInputDispatcher *self)
{
    self->backend = grdc_x11_input_new();
    self->active = FALSE;
}

GrdcInputDispatcher *
grdc_input_dispatcher_new(void)
{
    return g_object_new(GRDC_TYPE_INPUT_DISPATCHER, NULL);
}

gboolean
grdc_input_dispatcher_start(GrdcInputDispatcher *self, guint width, guint height, GError **error)
{
    g_return_val_if_fail(GRDC_IS_INPUT_DISPATCHER(self), FALSE);

    if (!grdc_x11_input_start(self->backend, error))
    {
        return FALSE;
    }

    grdc_x11_input_update_desktop_size(self->backend, width, height);
    self->active = TRUE;
    return TRUE;
}

void
grdc_input_dispatcher_stop(GrdcInputDispatcher *self)
{
    g_return_if_fail(GRDC_IS_INPUT_DISPATCHER(self));
    grdc_x11_input_stop(self->backend);
    self->active = FALSE;
}

void
grdc_input_dispatcher_update_desktop_size(GrdcInputDispatcher *self, guint width, guint height)
{
    g_return_if_fail(GRDC_IS_INPUT_DISPATCHER(self));
    grdc_x11_input_update_desktop_size(self->backend, width, height);
}

gboolean
grdc_input_dispatcher_handle_keyboard(GrdcInputDispatcher *self,
                                      guint16 flags,
                                      guint8 scancode,
                                      GError **error)
{
    g_return_val_if_fail(GRDC_IS_INPUT_DISPATCHER(self), FALSE);
    return grdc_x11_input_inject_keyboard(self->backend, flags, scancode, error);
}

gboolean
grdc_input_dispatcher_handle_unicode(GrdcInputDispatcher *self,
                                     guint16 flags,
                                     guint16 codepoint,
                                     GError **error)
{
    g_return_val_if_fail(GRDC_IS_INPUT_DISPATCHER(self), FALSE);
    return grdc_x11_input_inject_unicode(self->backend, flags, codepoint, error);
}

gboolean
grdc_input_dispatcher_handle_pointer(GrdcInputDispatcher *self,
                                     guint16 flags,
                                     guint16 x,
                                     guint16 y,
                                     GError **error)
{
    g_return_val_if_fail(GRDC_IS_INPUT_DISPATCHER(self), FALSE);
    return grdc_x11_input_inject_pointer(self->backend, flags, x, y, error);
}

void
grdc_input_dispatcher_flush(GrdcInputDispatcher *self)
{
    (void)self;
    /* No buffered events at the moment; maintained for API symmetry. */
}
