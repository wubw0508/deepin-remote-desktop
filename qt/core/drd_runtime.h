#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtGlobal>

struct rdpContext;
struct rdpSettings;
struct RdpgfxServerContext;

class DrdQtRuntime : public QObject {
public:
  enum class RuntimeMode { User = 0, System, Handover };

  enum class FrameTransport { SurfaceBits = 0, GraphicsPipeline };

  struct EncodingOptions {
    quint32 width = 0;
    quint32 height = 0;
    quint32 mode = 0;
    bool enable_frame_diff = false;
    quint32 h264_bitrate = 0;
    quint32 h264_framerate = 0;
    quint32 h264_qp = 0;
    bool h264_hw_accel = false;
    bool h264_vm_support = false;
    double gfx_large_change_threshold = 0.0;
    quint32 gfx_progressive_refresh_interval = 0;
    quint32 gfx_progressive_refresh_timeout_ms = 0;
  };

  explicit DrdQtRuntime(QObject *parent = nullptr);

  const QString &module_name() const;

  QObject *drd_application_new();
  int drd_application_run(QObject *application, const QStringList &arguments,
                          QString *error_message);

  QObject *drd_config_new();
  QObject *drd_config_new_from_file(const QString &path,
                                    QString *error_message);
  bool
  drd_config_merge_cli(QObject *config, const QString &bind_address, int port,
                       const QString &cert_path, const QString &key_path,
                       const QString &nla_username, const QString &nla_password,
                       bool cli_enable_nla, bool cli_disable_nla,
                       const QString &runtime_mode_name, int width, int height,
                       const QString &encoder_mode, int diff_override,
                       int capture_target_fps, int capture_stats_interval_sec,
                       QString *error_message);

  QString drd_config_get_bind_address(QObject *config) const;
  quint16 drd_config_get_port(QObject *config) const;
  QString drd_config_get_certificate_path(QObject *config) const;
  QString drd_config_get_private_key_path(QObject *config) const;
  QString drd_config_get_nla_username(QObject *config) const;
  QString drd_config_get_nla_password(QObject *config) const;
  bool drd_config_is_nla_enabled(QObject *config) const;
  RuntimeMode drd_config_get_runtime_mode(QObject *config) const;
  QString drd_config_get_pam_service(QObject *config) const;
  quint32 drd_config_get_capture_width(QObject *config) const;
  quint32 drd_config_get_capture_height(QObject *config) const;
  quint32 drd_config_get_capture_target_fps(QObject *config) const;
  quint32 drd_config_get_capture_stats_interval_sec(QObject *config) const;
  EncodingOptions drd_config_get_encoding_options(QObject *config) const;

  QObject *drd_server_runtime_new();
  QObject *drd_server_runtime_get_capture(QObject *runtime) const;
  QObject *drd_server_runtime_get_encoder(QObject *runtime) const;
  QObject *drd_server_runtime_get_input(QObject *runtime) const;
  bool
  drd_server_runtime_prepare_stream(QObject *runtime,
                                    const EncodingOptions &encoding_options,
                                    QString *error_message);
  void drd_server_runtime_stop(QObject *runtime);
  bool drd_server_runtime_pull_encoded_frame_surface_gfx(
      QObject *runtime, rdpSettings *settings, RdpgfxServerContext *context,
      quint16 surface_id, qint64 timeout_us, quint32 frame_id, bool *h264,
      QString *error_message);
  bool drd_server_runtime_send_cached_frame_surface_gfx(
      QObject *runtime, rdpSettings *settings, RdpgfxServerContext *context,
      quint16 surface_id, quint32 frame_id, bool *h264, QString *error_message);
  bool drd_server_runtime_pull_encoded_frame_surface_bit(
      QObject *runtime, rdpContext *context, quint32 frame_id,
      qsizetype max_payload, qint64 timeout_us, QString *error_message);
  void drd_server_runtime_set_transport(QObject *runtime,
                                        FrameTransport transport);
  FrameTransport drd_server_runtime_get_transport(QObject *runtime) const;
  bool
  drd_server_runtime_get_encoding_options(QObject *runtime,
                                          EncodingOptions *out_options) const;
  void drd_server_runtime_set_encoding_options(
      QObject *runtime, const EncodingOptions &encoding_options);
  bool drd_server_runtime_is_stream_running(QObject *runtime) const;
  void drd_server_runtime_set_tls_credentials(QObject *runtime,
                                              QObject *credentials);
  QObject *drd_server_runtime_get_tls_credentials(QObject *runtime) const;
  void drd_server_runtime_request_keyframe(QObject *runtime);

  bool drd_runtime_encoder_prepare(QObject *runtime, quint32 codecs,
                                   rdpSettings *settings);

private:
  QString module_name_;
};
