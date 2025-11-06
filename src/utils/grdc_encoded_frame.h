#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
    GRDC_FRAME_CODEC_RAW = 0,
    GRDC_FRAME_CODEC_RFX,
    GRDC_FRAME_CODEC_H264
} GrdcFrameCodec;

#define GRDC_TYPE_ENCODED_FRAME (grdc_encoded_frame_get_type())
G_DECLARE_FINAL_TYPE(GrdcEncodedFrame, grdc_encoded_frame, GRDC, ENCODED_FRAME, GObject)

GrdcEncodedFrame *grdc_encoded_frame_new(void);

void grdc_encoded_frame_configure(GrdcEncodedFrame *self,
                                  guint width,
                                  guint height,
                                  guint stride,
                                  gboolean is_bottom_up,
                                  guint64 timestamp,
                                  GrdcFrameCodec codec);

void grdc_encoded_frame_set_quality(GrdcEncodedFrame *self, guint8 quality, guint8 qp, gboolean is_keyframe);

guint8 *grdc_encoded_frame_ensure_capacity(GrdcEncodedFrame *self, gsize size);
const guint8 *grdc_encoded_frame_get_data(GrdcEncodedFrame *self, gsize *size);

guint grdc_encoded_frame_get_width(GrdcEncodedFrame *self);
guint grdc_encoded_frame_get_height(GrdcEncodedFrame *self);
guint grdc_encoded_frame_get_stride(GrdcEncodedFrame *self);
gboolean grdc_encoded_frame_get_is_bottom_up(GrdcEncodedFrame *self);
guint64 grdc_encoded_frame_get_timestamp(GrdcEncodedFrame *self);
GrdcFrameCodec grdc_encoded_frame_get_codec(GrdcEncodedFrame *self);
gboolean grdc_encoded_frame_is_keyframe(GrdcEncodedFrame *self);
guint8 grdc_encoded_frame_get_quality(GrdcEncodedFrame *self);
guint8 grdc_encoded_frame_get_qp(GrdcEncodedFrame *self);

G_END_DECLS
