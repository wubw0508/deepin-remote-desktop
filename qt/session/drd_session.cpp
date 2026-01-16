#include "drd_session.h"

DrdQtSession::DrdQtSession(QObject *parent)
    : QObject(parent), module_name_(QStringLiteral("session")) {}

const QString &DrdQtSession::module_name() const { return module_name_; }

QObject *DrdQtSession::drd_rdp_session_new(freerdp_peer *peer) {
  Q_UNUSED(peer);
  // TODO: FreeRDP peer session initialization.
  return new QObject(this);
}

void DrdQtSession::drd_rdp_session_set_peer_address(
    QObject *session, const QString &peer_address) {
  Q_UNUSED(session);
  Q_UNUSED(peer_address);
}

QString DrdQtSession::drd_rdp_session_get_peer_address(QObject *session) const {
  Q_UNUSED(session);
  return QString();
}

void DrdQtSession::drd_rdp_session_set_peer_state(QObject *session,
                                                  const QString &state) {
  Q_UNUSED(session);
  Q_UNUSED(state);
}

void DrdQtSession::drd_rdp_session_set_runtime(QObject *session,
                                               QObject *runtime) {
  Q_UNUSED(session);
  Q_UNUSED(runtime);
}

void DrdQtSession::drd_rdp_session_set_virtual_channel_manager(QObject *session,
                                                               void *vcm) {
  Q_UNUSED(session);
  Q_UNUSED(vcm);
}

void DrdQtSession::drd_rdp_session_set_closed_callback(
    QObject *session, const std::function<void(QObject *, QObject *)> &callback,
    QObject *user_data) {
  Q_UNUSED(session);
  Q_UNUSED(callback);
  Q_UNUSED(user_data);
}

void DrdQtSession::drd_rdp_session_set_passive_mode(QObject *session,
                                                    bool passive) {
  Q_UNUSED(session);
  Q_UNUSED(passive);
}

void DrdQtSession::drd_rdp_session_attach_local_session(
    QObject *session, QObject *local_session) {
  Q_UNUSED(session);
  Q_UNUSED(local_session);
}

bool DrdQtSession::drd_rdp_session_post_connect(QObject *session) {
  Q_UNUSED(session);
  // TODO: FreeRDP post-connect callbacks.
  return true;
}

bool DrdQtSession::drd_rdp_session_activate(QObject *session) {
  Q_UNUSED(session);
  // TODO: FreeRDP activate callbacks.
  return true;
}

bool DrdQtSession::drd_rdp_session_pump(QObject *session) {
  Q_UNUSED(session);
  // TODO: FreeRDP event pump loop.
  return true;
}

void DrdQtSession::drd_rdp_session_disconnect(QObject *session,
                                              const QString &reason) {
  Q_UNUSED(session);
  Q_UNUSED(reason);
  // TODO: FreeRDP disconnect flow.
}

void DrdQtSession::drd_rdp_session_notify_error(QObject *session,
                                                RdpSessionError error) {
  Q_UNUSED(session);
  Q_UNUSED(error);
}

bool DrdQtSession::drd_rdp_session_start_event_thread(QObject *session) {
  Q_UNUSED(session);
  return true;
}

void DrdQtSession::drd_rdp_session_stop_event_thread(QObject *session) {
  Q_UNUSED(session);
}

bool DrdQtSession::drd_rdp_session_send_server_redirection(
    QObject *session, const QString &routing_token, const QString &username,
    const QString &password, const QString &certificate) {
  Q_UNUSED(session);
  Q_UNUSED(routing_token);
  Q_UNUSED(username);
  Q_UNUSED(password);
  Q_UNUSED(certificate);
  // TODO: FreeRDP server redirection PDU.
  return true;
}

bool DrdQtSession::drd_rdp_session_client_is_mstsc(QObject *session) {
  Q_UNUSED(session);
  // TODO: FreeRDP client detection.
  return false;
}

bool DrdQtSession::drd_rdp_session_get_peer_resolution(QObject *session,
                                                       quint32 *out_width,
                                                       quint32 *out_height) {
  Q_UNUSED(session);
  // TODO: FreeRDP peer monitor data.
  if (out_width) {
    *out_width = 0;
  }
  if (out_height) {
    *out_height = 0;
  }
  return false;
}

QObject *DrdQtSession::drd_rdp_graphics_pipeline_new(freerdp_peer *peer,
                                                     void *vcm,
                                                     QObject *runtime,
                                                     quint16 surface_width,
                                                     quint16 surface_height) {
  Q_UNUSED(peer);
  Q_UNUSED(vcm);
  Q_UNUSED(runtime);
  Q_UNUSED(surface_width);
  Q_UNUSED(surface_height);
  // TODO: FreeRDP Rdpgfx pipeline setup.
  return new QObject(this);
}

bool DrdQtSession::drd_rdp_graphics_pipeline_maybe_init(QObject *pipeline) {
  Q_UNUSED(pipeline);
  // TODO: FreeRDP Rdpgfx init sequence.
  return true;
}

bool DrdQtSession::drd_rdp_graphics_pipeline_is_ready(QObject *pipeline) {
  Q_UNUSED(pipeline);
  // TODO: FreeRDP Rdpgfx ready check.
  return false;
}

bool DrdQtSession::drd_rdp_graphics_pipeline_can_submit(QObject *pipeline) {
  Q_UNUSED(pipeline);
  // TODO: FreeRDP frame submit throttling.
  return false;
}

bool DrdQtSession::drd_rdp_graphics_pipeline_wait_for_capacity(
    QObject *pipeline, qint64 timeout_us) {
  Q_UNUSED(pipeline);
  Q_UNUSED(timeout_us);
  // TODO: FreeRDP Rdpgfx capacity wait.
  return false;
}

quint16
DrdQtSession::drd_rdp_graphics_pipeline_get_surface_id(QObject *pipeline) {
  Q_UNUSED(pipeline);
  // TODO: FreeRDP surface ID lookup.
  return 0;
}

void DrdQtSession::drd_rdp_graphics_pipeline_out_frame_change(QObject *pipeline,
                                                              bool add) {
  Q_UNUSED(pipeline);
  Q_UNUSED(add);
  // TODO: FreeRDP outstanding frame tracking.
}

RdpgfxServerContext *DrdQtSession::drd_rdpgfx_get_context(QObject *pipeline) {
  Q_UNUSED(pipeline);
  // TODO: FreeRDP Rdpgfx context access.
  return nullptr;
}

void DrdQtSession::drd_rdp_graphics_pipeline_set_last_frame_mode(
    QObject *pipeline, bool h264) {
  Q_UNUSED(pipeline);
  Q_UNUSED(h264);
  // TODO: FreeRDP frame mode tracking.
}
