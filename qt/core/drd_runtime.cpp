#include "qt/core/drd_runtime.h"

DrdQtRuntime::DrdQtRuntime(QObject *parent)
    : QObject(parent), module_name_(QStringLiteral("core")) {}

const QString &DrdQtRuntime::module_name() const { return module_name_; }

QObject *DrdQtRuntime::drd_application_new() { return new QObject(this); }

int DrdQtRuntime::drd_application_run(QObject *application,
                                      const QStringList &arguments,
                                      QString *error_message) {
  Q_UNUSED(application);
  Q_UNUSED(arguments);
  if (error_message) {
    error_message->clear();
  }
  return 0;
}

QObject *DrdQtRuntime::drd_config_new() { return new QObject(this); }

QObject *DrdQtRuntime::drd_config_new_from_file(const QString &path,
                                                QString *error_message) {
  Q_UNUSED(path);
  if (error_message) {
    error_message->clear();
  }
  return new QObject(this);
}

bool DrdQtRuntime::drd_config_merge_cli(
    QObject *config, const QString &bind_address, int port,
    const QString &cert_path, const QString &key_path,
    const QString &nla_username, const QString &nla_password,
    bool cli_enable_nla, bool cli_disable_nla, const QString &runtime_mode_name,
    int width, int height, const QString &encoder_mode, int diff_override,
    int capture_target_fps, int capture_stats_interval_sec,
    QString *error_message) {
  Q_UNUSED(config);
  Q_UNUSED(bind_address);
  Q_UNUSED(port);
  Q_UNUSED(cert_path);
  Q_UNUSED(key_path);
  Q_UNUSED(nla_username);
  Q_UNUSED(nla_password);
  Q_UNUSED(cli_enable_nla);
  Q_UNUSED(cli_disable_nla);
  Q_UNUSED(runtime_mode_name);
  Q_UNUSED(width);
  Q_UNUSED(height);
  Q_UNUSED(encoder_mode);
  Q_UNUSED(diff_override);
  Q_UNUSED(capture_target_fps);
  Q_UNUSED(capture_stats_interval_sec);
  if (error_message) {
    error_message->clear();
  }
  return true;
}

QString DrdQtRuntime::drd_config_get_bind_address(QObject *config) const {
  Q_UNUSED(config);
  return QString();
}

quint16 DrdQtRuntime::drd_config_get_port(QObject *config) const {
  Q_UNUSED(config);
  return 0;
}

QString DrdQtRuntime::drd_config_get_certificate_path(QObject *config) const {
  Q_UNUSED(config);
  return QString();
}

QString DrdQtRuntime::drd_config_get_private_key_path(QObject *config) const {
  Q_UNUSED(config);
  return QString();
}

QString DrdQtRuntime::drd_config_get_nla_username(QObject *config) const {
  Q_UNUSED(config);
  return QString();
}

QString DrdQtRuntime::drd_config_get_nla_password(QObject *config) const {
  Q_UNUSED(config);
  return QString();
}

bool DrdQtRuntime::drd_config_is_nla_enabled(QObject *config) const {
  Q_UNUSED(config);
  return false;
}

DrdQtRuntime::RuntimeMode
DrdQtRuntime::drd_config_get_runtime_mode(QObject *config) const {
  Q_UNUSED(config);
  return RuntimeMode::User;
}

QString DrdQtRuntime::drd_config_get_pam_service(QObject *config) const {
  Q_UNUSED(config);
  return QString();
}

quint32 DrdQtRuntime::drd_config_get_capture_width(QObject *config) const {
  Q_UNUSED(config);
  return 0;
}

quint32 DrdQtRuntime::drd_config_get_capture_height(QObject *config) const {
  Q_UNUSED(config);
  return 0;
}

quint32 DrdQtRuntime::drd_config_get_capture_target_fps(QObject *config) const {
  Q_UNUSED(config);
  return 0;
}

quint32
DrdQtRuntime::drd_config_get_capture_stats_interval_sec(QObject *config) const {
  Q_UNUSED(config);
  return 0;
}

DrdQtRuntime::EncodingOptions
DrdQtRuntime::drd_config_get_encoding_options(QObject *config) const {
  Q_UNUSED(config);
  return EncodingOptions();
}

QObject *DrdQtRuntime::drd_server_runtime_new() { return new QObject(this); }

QObject *DrdQtRuntime::drd_server_runtime_get_capture(QObject *runtime) const {
  Q_UNUSED(runtime);
  return nullptr;
}

QObject *DrdQtRuntime::drd_server_runtime_get_encoder(QObject *runtime) const {
  Q_UNUSED(runtime);
  return nullptr;
}

QObject *DrdQtRuntime::drd_server_runtime_get_input(QObject *runtime) const {
  Q_UNUSED(runtime);
  return nullptr;
}

bool DrdQtRuntime::drd_server_runtime_prepare_stream(
    QObject *runtime, const EncodingOptions &encoding_options,
    QString *error_message) {
  Q_UNUSED(runtime);
  Q_UNUSED(encoding_options);
  if (error_message) {
    error_message->clear();
  }
  return true;
}

void DrdQtRuntime::drd_server_runtime_stop(QObject *runtime) {
  Q_UNUSED(runtime);
}

bool DrdQtRuntime::drd_server_runtime_pull_encoded_frame_surface_gfx(
    QObject *runtime, rdpSettings *settings, RdpgfxServerContext *context,
    quint16 surface_id, qint64 timeout_us, quint32 frame_id, bool *h264,
    QString *error_message) {
  Q_UNUSED(runtime);
  Q_UNUSED(settings);
  Q_UNUSED(context);
  Q_UNUSED(surface_id);
  Q_UNUSED(timeout_us);
  Q_UNUSED(frame_id);
  if (h264) {
    *h264 = false;
  }
  if (error_message) {
    error_message->clear();
  }
  return false;
}

bool DrdQtRuntime::drd_server_runtime_send_cached_frame_surface_gfx(
    QObject *runtime, rdpSettings *settings, RdpgfxServerContext *context,
    quint16 surface_id, quint32 frame_id, bool *h264, QString *error_message) {
  Q_UNUSED(runtime);
  Q_UNUSED(settings);
  Q_UNUSED(context);
  Q_UNUSED(surface_id);
  Q_UNUSED(frame_id);
  if (h264) {
    *h264 = false;
  }
  if (error_message) {
    error_message->clear();
  }
  return false;
}

bool DrdQtRuntime::drd_server_runtime_pull_encoded_frame_surface_bit(
    QObject *runtime, rdpContext *context, quint32 frame_id,
    qsizetype max_payload, qint64 timeout_us, QString *error_message) {
  Q_UNUSED(runtime);
  Q_UNUSED(context);
  Q_UNUSED(frame_id);
  Q_UNUSED(max_payload);
  Q_UNUSED(timeout_us);
  if (error_message) {
    error_message->clear();
  }
  return false;
}

void DrdQtRuntime::drd_server_runtime_set_transport(QObject *runtime,
                                                    FrameTransport transport) {
  Q_UNUSED(runtime);
  Q_UNUSED(transport);
}

DrdQtRuntime::FrameTransport
DrdQtRuntime::drd_server_runtime_get_transport(QObject *runtime) const {
  Q_UNUSED(runtime);
  return FrameTransport::SurfaceBits;
}

bool DrdQtRuntime::drd_server_runtime_get_encoding_options(
    QObject *runtime, EncodingOptions *out_options) const {
  Q_UNUSED(runtime);
  if (out_options) {
    *out_options = EncodingOptions();
  }
  return false;
}

void DrdQtRuntime::drd_server_runtime_set_encoding_options(
    QObject *runtime, const EncodingOptions &encoding_options) {
  Q_UNUSED(runtime);
  Q_UNUSED(encoding_options);
}

bool DrdQtRuntime::drd_server_runtime_is_stream_running(
    QObject *runtime) const {
  Q_UNUSED(runtime);
  return false;
}

void DrdQtRuntime::drd_server_runtime_set_tls_credentials(
    QObject *runtime, QObject *credentials) {
  Q_UNUSED(runtime);
  Q_UNUSED(credentials);
}

QObject *
DrdQtRuntime::drd_server_runtime_get_tls_credentials(QObject *runtime) const {
  Q_UNUSED(runtime);
  return nullptr;
}

void DrdQtRuntime::drd_server_runtime_request_keyframe(QObject *runtime) {
  Q_UNUSED(runtime);
}

bool DrdQtRuntime::drd_runtime_encoder_prepare(QObject *runtime, quint32 codecs,
                                               rdpSettings *settings) {
  Q_UNUSED(runtime);
  Q_UNUSED(codecs);
  Q_UNUSED(settings);
  return false;
}
