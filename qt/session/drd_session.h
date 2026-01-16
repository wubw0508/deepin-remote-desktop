#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <functional>

struct freerdp_peer;
struct RdpgfxServerContext;

class DrdQtSession : public QObject {
public:
  enum class RdpSessionError {
    None = 0,
    BadCaps,
    BadMonitorData,
    CloseStackOnDriverFailure,
    GraphicsSubsystemFailed,
    ServerRedirection
  };

  explicit DrdQtSession(QObject *parent = nullptr);

  const QString &module_name() const;

  QObject *drd_rdp_session_new(freerdp_peer *peer);
  void drd_rdp_session_set_peer_address(QObject *session,
                                        const QString &peer_address);
  QString drd_rdp_session_get_peer_address(QObject *session) const;
  void drd_rdp_session_set_peer_state(QObject *session, const QString &state);
  void drd_rdp_session_set_runtime(QObject *session, QObject *runtime);
  void drd_rdp_session_set_virtual_channel_manager(QObject *session, void *vcm);
  void drd_rdp_session_set_closed_callback(
      QObject *session,
      const std::function<void(QObject *, QObject *)> &callback,
      QObject *user_data);
  void drd_rdp_session_set_passive_mode(QObject *session, bool passive);
  void drd_rdp_session_attach_local_session(QObject *session,
                                            QObject *local_session);
  bool drd_rdp_session_post_connect(QObject *session);
  bool drd_rdp_session_activate(QObject *session);
  bool drd_rdp_session_pump(QObject *session);
  void drd_rdp_session_disconnect(QObject *session, const QString &reason);
  void drd_rdp_session_notify_error(QObject *session, RdpSessionError error);
  bool drd_rdp_session_start_event_thread(QObject *session);
  void drd_rdp_session_stop_event_thread(QObject *session);
  bool drd_rdp_session_send_server_redirection(QObject *session,
                                               const QString &routing_token,
                                               const QString &username,
                                               const QString &password,
                                               const QString &certificate);
  bool drd_rdp_session_client_is_mstsc(QObject *session);
  bool drd_rdp_session_get_peer_resolution(QObject *session, quint32 *out_width,
                                           quint32 *out_height);

  QObject *drd_rdp_graphics_pipeline_new(freerdp_peer *peer, void *vcm,
                                         QObject *runtime,
                                         quint16 surface_width,
                                         quint16 surface_height);
  bool drd_rdp_graphics_pipeline_maybe_init(QObject *pipeline);
  bool drd_rdp_graphics_pipeline_is_ready(QObject *pipeline);
  bool drd_rdp_graphics_pipeline_can_submit(QObject *pipeline);
  bool drd_rdp_graphics_pipeline_wait_for_capacity(QObject *pipeline,
                                                   qint64 timeout_us);
  quint16 drd_rdp_graphics_pipeline_get_surface_id(QObject *pipeline);
  void drd_rdp_graphics_pipeline_out_frame_change(QObject *pipeline, bool add);
  RdpgfxServerContext *drd_rdpgfx_get_context(QObject *pipeline);
  void drd_rdp_graphics_pipeline_set_last_frame_mode(QObject *pipeline,
                                                     bool h264);

private:
  QString module_name_;
};
