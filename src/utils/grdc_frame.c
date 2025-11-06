#include "utils/grdc_frame.h"

#include <string.h>

struct _GrdcFrame
{
    GObject parent_instance;

    GByteArray *pixels;
    guint width;
    guint height;
    guint stride;
    guint64 timestamp;
};

G_DEFINE_TYPE(GrdcFrame, grdc_frame, G_TYPE_OBJECT)

static void
grdc_frame_dispose(GObject *object)
{
    GrdcFrame *self = GRDC_FRAME(object);
    g_clear_pointer(&self->pixels, g_byte_array_unref);
    G_OBJECT_CLASS(grdc_frame_parent_class)->dispose(object);
}

static void
grdc_frame_class_init(GrdcFrameClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = grdc_frame_dispose;
}

static void
grdc_frame_init(GrdcFrame *self)
{
    self->pixels = g_byte_array_new();
}

GrdcFrame *
grdc_frame_new(void)
{
    return g_object_new(GRDC_TYPE_FRAME, NULL);
}

void
grdc_frame_configure(GrdcFrame *self,
                     guint width,
                     guint height,
                     guint stride,
                     guint64 timestamp)
{
    g_return_if_fail(GRDC_IS_FRAME(self));

    self->width = width;
    self->height = height;
    self->stride = stride;
    self->timestamp = timestamp;
}

guint
grdc_frame_get_width(GrdcFrame *self)
{
    g_return_val_if_fail(GRDC_IS_FRAME(self), 0);
    return self->width;
}

guint
grdc_frame_get_height(GrdcFrame *self)
{
    g_return_val_if_fail(GRDC_IS_FRAME(self), 0);
    return self->height;
}

guint
grdc_frame_get_stride(GrdcFrame *self)
{
    g_return_val_if_fail(GRDC_IS_FRAME(self), 0);
    return self->stride;
}

guint64
grdc_frame_get_timestamp(GrdcFrame *self)
{
    g_return_val_if_fail(GRDC_IS_FRAME(self), 0);
    return self->timestamp;
}

guint8 *
grdc_frame_ensure_capacity(GrdcFrame *self, gsize size)
{
    g_return_val_if_fail(GRDC_IS_FRAME(self), NULL);

    if (self->pixels->len != size)
    {
        g_byte_array_set_size(self->pixels, size);
    }

    return self->pixels->data;
}

const guint8 *
grdc_frame_get_data(GrdcFrame *self, gsize *size)
{
    g_return_val_if_fail(GRDC_IS_FRAME(self), NULL);

    if (size != NULL)
    {
        *size = self->pixels->len;
    }

    return self->pixels->data;
}
