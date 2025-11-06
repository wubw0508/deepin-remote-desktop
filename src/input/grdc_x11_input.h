#pragma once

#include <glib-object.h>

#include "utils/grdc_frame.h"

G_BEGIN_DECLS

#define GRDC_TYPE_X11_INPUT (grdc_x11_input_get_type())
G_DECLARE_FINAL_TYPE(GrdcX11Input, grdc_x11_input, GRDC, X11_INPUT, GObject)

GrdcX11Input *grdc_x11_input_new(void);

gboolean grdc_x11_input_start(GrdcX11Input *self, GError **error);
void grdc_x11_input_stop(GrdcX11Input *self);

void grdc_x11_input_update_desktop_size(GrdcX11Input *self, guint width, guint height);

gboolean grdc_x11_input_inject_keyboard(GrdcX11Input *self, guint16 flags, guint8 scancode, GError **error);
gboolean grdc_x11_input_inject_unicode(GrdcX11Input *self, guint16 flags, guint16 codepoint, GError **error);
gboolean grdc_x11_input_inject_pointer(GrdcX11Input *self,
                                       guint16 flags,
                                       guint16 x,
                                       guint16 y,
                                       GError **error);

G_END_DECLS
