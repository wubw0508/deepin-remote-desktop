#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define GRDC_TYPE_INPUT_DISPATCHER (grdc_input_dispatcher_get_type())
G_DECLARE_FINAL_TYPE(GrdcInputDispatcher, grdc_input_dispatcher, GRDC, INPUT_DISPATCHER, GObject)

GrdcInputDispatcher *grdc_input_dispatcher_new(void);

gboolean grdc_input_dispatcher_start(GrdcInputDispatcher *self, guint width, guint height, GError **error);
void grdc_input_dispatcher_stop(GrdcInputDispatcher *self);
void grdc_input_dispatcher_update_desktop_size(GrdcInputDispatcher *self, guint width, guint height);

gboolean grdc_input_dispatcher_handle_keyboard(GrdcInputDispatcher *self,
                                               guint16 flags,
                                               guint8 scancode,
                                               GError **error);
gboolean grdc_input_dispatcher_handle_unicode(GrdcInputDispatcher *self,
                                              guint16 flags,
                                              guint16 codepoint,
                                              GError **error);
gboolean grdc_input_dispatcher_handle_pointer(GrdcInputDispatcher *self,
                                              guint16 flags,
                                              guint16 x,
                                              guint16 y,
                                              GError **error);

void grdc_input_dispatcher_flush(GrdcInputDispatcher *self);

G_END_DECLS
