#pragma once

#include <glib-object.h>

#include "utils/grdc_frame_queue.h"
#include "utils/grdc_frame.h"

G_BEGIN_DECLS

#define GRDC_TYPE_CAPTURE_MANAGER (grdc_capture_manager_get_type())
G_DECLARE_FINAL_TYPE(GrdcCaptureManager, grdc_capture_manager, GRDC, CAPTURE_MANAGER, GObject)

GrdcCaptureManager *grdc_capture_manager_new(void);
gboolean grdc_capture_manager_start(GrdcCaptureManager *self, guint width, guint height, GError **error);
void grdc_capture_manager_stop(GrdcCaptureManager *self);
gboolean grdc_capture_manager_is_running(GrdcCaptureManager *self);
GrdcFrameQueue *grdc_capture_manager_get_queue(GrdcCaptureManager *self);
gboolean grdc_capture_manager_wait_frame(GrdcCaptureManager *self,
                                         gint64 timeout_us,
                                         GrdcFrame **out_frame,
                                         GError **error);

G_END_DECLS
