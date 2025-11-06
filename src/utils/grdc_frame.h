#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define GRDC_TYPE_FRAME (grdc_frame_get_type())
G_DECLARE_FINAL_TYPE(GrdcFrame, grdc_frame, GRDC, FRAME, GObject)

GrdcFrame *grdc_frame_new(void);

void grdc_frame_configure(GrdcFrame *self,
                          guint width,
                          guint height,
                          guint stride,
                          guint64 timestamp);

guint grdc_frame_get_width(GrdcFrame *self);
guint grdc_frame_get_height(GrdcFrame *self);
guint grdc_frame_get_stride(GrdcFrame *self);
guint64 grdc_frame_get_timestamp(GrdcFrame *self);

/**
 * grdc_frame_ensure_capacity:
 * @self: the frame
 * @size: required byte size
 *
 * Ensures the internal storage has exactly @size bytes allocated.
 * Returns a pointer to the writable buffer for direct population.
 */
guint8 *grdc_frame_ensure_capacity(GrdcFrame *self, gsize size);

const guint8 *grdc_frame_get_data(GrdcFrame *self, gsize *size);

G_END_DECLS
