#include "utils/grdc_encoded_frame.h"

struct _GrdcEncodedFrame
{
    GObject parent_instance;

    GByteArray *payload;
    guint width;
    guint height;
    guint stride;
    gboolean is_bottom_up;
    guint64 timestamp;
    GrdcFrameCodec codec;
    guint8 quality;
    guint8 qp;
    gboolean is_keyframe;
};

G_DEFINE_TYPE(GrdcEncodedFrame, grdc_encoded_frame, G_TYPE_OBJECT)

static void
grdc_encoded_frame_dispose(GObject *object)
{
    GrdcEncodedFrame *self = GRDC_ENCODED_FRAME(object);
    g_clear_pointer(&self->payload, g_byte_array_unref);
    G_OBJECT_CLASS(grdc_encoded_frame_parent_class)->dispose(object);
}

static void
grdc_encoded_frame_class_init(GrdcEncodedFrameClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = grdc_encoded_frame_dispose;
}

static void
grdc_encoded_frame_init(GrdcEncodedFrame *self)
{
    self->payload = g_byte_array_new();
    self->quality = 100;
    self->qp = 0;
    self->codec = GRDC_FRAME_CODEC_RAW;
    self->is_bottom_up = FALSE;
    self->is_keyframe = TRUE;
}

GrdcEncodedFrame *
grdc_encoded_frame_new(void)
{
    return g_object_new(GRDC_TYPE_ENCODED_FRAME, NULL);
}

void
grdc_encoded_frame_configure(GrdcEncodedFrame *self,
                             guint width,
                             guint height,
                             guint stride,
                             gboolean is_bottom_up,
                             guint64 timestamp,
                             GrdcFrameCodec codec)
{
    g_return_if_fail(GRDC_IS_ENCODED_FRAME(self));

    self->width = width;
    self->height = height;
    self->stride = stride;
    self->is_bottom_up = is_bottom_up;
    self->timestamp = timestamp;
    self->codec = codec;
}

void
grdc_encoded_frame_set_quality(GrdcEncodedFrame *self, guint8 quality, guint8 qp, gboolean is_keyframe)
{
    g_return_if_fail(GRDC_IS_ENCODED_FRAME(self));

    self->quality = quality;
    self->qp = qp;
    self->is_keyframe = is_keyframe;
}

guint8 *
grdc_encoded_frame_ensure_capacity(GrdcEncodedFrame *self, gsize size)
{
    g_return_val_if_fail(GRDC_IS_ENCODED_FRAME(self), NULL);

    if (self->payload->len != size)
    {
        g_byte_array_set_size(self->payload, size);
    }

    return self->payload->data;
}

const guint8 *
grdc_encoded_frame_get_data(GrdcEncodedFrame *self, gsize *size)
{
    g_return_val_if_fail(GRDC_IS_ENCODED_FRAME(self), NULL);

    if (size != NULL)
    {
        *size = self->payload->len;
    }

    return self->payload->data;
}

guint
grdc_encoded_frame_get_width(GrdcEncodedFrame *self)
{
    g_return_val_if_fail(GRDC_IS_ENCODED_FRAME(self), 0);
    return self->width;
}

guint
grdc_encoded_frame_get_height(GrdcEncodedFrame *self)
{
    g_return_val_if_fail(GRDC_IS_ENCODED_FRAME(self), 0);
    return self->height;
}

guint
grdc_encoded_frame_get_stride(GrdcEncodedFrame *self)
{
    g_return_val_if_fail(GRDC_IS_ENCODED_FRAME(self), 0);
    return self->stride;
}

gboolean
grdc_encoded_frame_get_is_bottom_up(GrdcEncodedFrame *self)
{
    g_return_val_if_fail(GRDC_IS_ENCODED_FRAME(self), FALSE);
    return self->is_bottom_up;
}

guint64
grdc_encoded_frame_get_timestamp(GrdcEncodedFrame *self)
{
    g_return_val_if_fail(GRDC_IS_ENCODED_FRAME(self), 0);
    return self->timestamp;
}

GrdcFrameCodec
grdc_encoded_frame_get_codec(GrdcEncodedFrame *self)
{
    g_return_val_if_fail(GRDC_IS_ENCODED_FRAME(self), GRDC_FRAME_CODEC_RAW);
    return self->codec;
}

gboolean
grdc_encoded_frame_is_keyframe(GrdcEncodedFrame *self)
{
    g_return_val_if_fail(GRDC_IS_ENCODED_FRAME(self), FALSE);
    return self->is_keyframe;
}

guint8
grdc_encoded_frame_get_quality(GrdcEncodedFrame *self)
{
    g_return_val_if_fail(GRDC_IS_ENCODED_FRAME(self), 0);
    return self->quality;
}

guint8
grdc_encoded_frame_get_qp(GrdcEncodedFrame *self)
{
    g_return_val_if_fail(GRDC_IS_ENCODED_FRAME(self), 0);
    return self->qp;
}
