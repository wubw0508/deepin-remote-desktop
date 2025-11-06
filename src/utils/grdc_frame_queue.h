#pragma once

#include <glib-object.h>

#include "utils/grdc_frame.h"

G_BEGIN_DECLS

#define GRDC_TYPE_FRAME_QUEUE (grdc_frame_queue_get_type())
G_DECLARE_FINAL_TYPE(GrdcFrameQueue, grdc_frame_queue, GRDC, FRAME_QUEUE, GObject)

GrdcFrameQueue *grdc_frame_queue_new(void);

void grdc_frame_queue_reset(GrdcFrameQueue *self);
void grdc_frame_queue_push(GrdcFrameQueue *self, GrdcFrame *frame);
gboolean grdc_frame_queue_wait(GrdcFrameQueue *self,
                               gint64 timeout_us,
                               GrdcFrame **out_frame);
void grdc_frame_queue_stop(GrdcFrameQueue *self);

G_END_DECLS
