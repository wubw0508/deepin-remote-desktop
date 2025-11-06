#pragma once

#include <glib-object.h>

#include "utils/grdc_frame_queue.h"

G_BEGIN_DECLS

#define GRDC_TYPE_X11_CAPTURE (grdc_x11_capture_get_type())
G_DECLARE_FINAL_TYPE(GrdcX11Capture, grdc_x11_capture, GRDC, X11_CAPTURE, GObject)

GrdcX11Capture *grdc_x11_capture_new(GrdcFrameQueue *queue);

gboolean grdc_x11_capture_start(GrdcX11Capture *self,
                                const gchar *display_name,
                                guint requested_width,
                                guint requested_height,
                                GError **error);

void grdc_x11_capture_stop(GrdcX11Capture *self);
gboolean grdc_x11_capture_is_running(GrdcX11Capture *self);

G_END_DECLS
