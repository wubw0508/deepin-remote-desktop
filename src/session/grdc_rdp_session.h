#pragma once

#include <sys/types.h>
#include <stdio.h>
#include <freerdp/listener.h>
#include <glib-object.h>

typedef struct _GrdcServerRuntime GrdcServerRuntime;

#include "utils/grdc_encoded_frame.h"

G_BEGIN_DECLS

#define GRDC_TYPE_RDP_SESSION (grdc_rdp_session_get_type())
G_DECLARE_FINAL_TYPE(GrdcRdpSession, grdc_rdp_session, GRDC, RDP_SESSION, GObject)

GrdcRdpSession *grdc_rdp_session_new(freerdp_peer *peer);
void grdc_rdp_session_set_peer_state(GrdcRdpSession *self, const gchar *state);
void grdc_rdp_session_set_runtime(GrdcRdpSession *self, GrdcServerRuntime *runtime);
BOOL grdc_rdp_session_post_connect(GrdcRdpSession *self);
BOOL grdc_rdp_session_activate(GrdcRdpSession *self);
BOOL grdc_rdp_session_pump(GrdcRdpSession *self);
void grdc_rdp_session_disconnect(GrdcRdpSession *self, const gchar *reason);
gboolean grdc_rdp_session_start_event_thread(GrdcRdpSession *self);
void grdc_rdp_session_stop_event_thread(GrdcRdpSession *self);

G_END_DECLS
