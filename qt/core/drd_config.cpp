#include "drd_config.h"

#include <QSettings>
#include <QFile>
#include <QDir>
#include <QDebug>

DrdQtConfig::DrdQtConfig(QObject *parent) : QObject(parent) {
  // 初始化默认配置
  values_["bind_address"] = "0.0.0.0";
  values_["port"] = 3390;
  values_["encoding_mode"] = "rfx";
  values_["enable_frame_diff"] = true;
  values_["nla_enabled"] = true;
  values_["runtime_mode"] = "user";
  values_["pam_service"] = "deepin-remote-desktop";
  values_["capture_target_fps"] = 60;
  values_["capture_stats_interval_sec"] = 5;
  // 初始化证书和密钥路径，避免合并时验证失败
  values_["certificate_path"] = QString();
  values_["private_key_path"] = QString();
}

bool DrdQtConfig::drd_config_load_from_file(const QString &path,
                                             QString *error_message) {
  qInfo() << "Loading configuration from file:" << path;
  
  if (!QFile::exists(path)) {
    if (error_message) {
      *error_message = QString("Config file not found: %1").arg(path);
    }
    qWarning() << "Config file not found:" << path;
    return false;
  }

  QSettings settings(path, QSettings::IniFormat);
  qInfo() << "Successfully opened config file:" << path;

  // 加载服务器配置
  if (settings.contains("server/bind_address")) {
    values_["bind_address"] = settings.value("server/bind_address").toString();
    qInfo() << "Loaded server/bind_address:" << values_["bind_address"].toString();
  }
  if (settings.contains("server/port")) {
    values_["port"] = settings.value("server/port").toInt();
    qInfo() << "Loaded server/port:" << values_["port"].toInt();
  }

  // 加载 TLS 配置
  if (settings.contains("tls/certificate")) {
    QString cert_path = settings.value("tls/certificate").toString();
    if (!QFile::exists(cert_path)) {
      cert_path = QDir(QFileInfo(path).absolutePath()).filePath(cert_path);
    }
    values_["certificate_path"] = cert_path;
    qInfo() << "Loaded tls/certificate:" << values_["certificate_path"].toString();
  }
  if (settings.contains("tls/private_key")) {
    QString key_path = settings.value("tls/private_key").toString();
    if (!QFile::exists(key_path)) {
      key_path = QDir(QFileInfo(path).absolutePath()).filePath(key_path);
    }
    values_["private_key_path"] = key_path;
    qInfo() << "Loaded tls/private_key:" << values_["private_key_path"].toString();
  }

  // 加载认证配置（NLA凭据）
  if (settings.contains("auth/username")) {
    values_["nla_username"] = settings.value("auth/username").toString();
    qInfo() << "Loaded auth/username:" << values_["nla_username"].toString();
  }
  if (settings.contains("auth/password")) {
    values_["nla_password"] = settings.value("auth/password").toString();
    qInfo() << "Loaded auth/password: [REDACTED]";
  }

  // 加载采集配置
  if (settings.contains("capture/width")) {
    values_["capture_width"] = settings.value("capture/width").toInt();
    qInfo() << "Loaded capture/width:" << values_["capture_width"].toInt();
  }
  if (settings.contains("capture/height")) {
    values_["capture_height"] = settings.value("capture/height").toInt();
    qInfo() << "Loaded capture/height:" << values_["capture_height"].toInt();
  }
  if (settings.contains("capture/target_fps")) {
    values_["capture_target_fps"] = settings.value("capture/target_fps").toInt();
    qInfo() << "Loaded capture/target_fps:" << values_["capture_target_fps"].toInt();
  }
  if (settings.contains("capture/stats_interval_sec")) {
    values_["capture_stats_interval_sec"] = settings.value("capture/stats_interval_sec").toInt();
    qInfo() << "Loaded capture/stats_interval_sec:" << values_["capture_stats_interval_sec"].toInt();
  }

  // 加载编码配置
  if (settings.contains("encoding/mode")) {
    values_["encoding_mode"] = settings.value("encoding/mode").toString();
    qInfo() << "Loaded encoding/mode:" << values_["encoding_mode"].toString();
  }
  if (settings.contains("encoding/enable_diff")) {
    values_["enable_frame_diff"] = settings.value("encoding/enable_diff").toBool();
    qInfo() << "Loaded encoding/enable_diff:" << values_["enable_frame_diff"].toBool();
  }

  // 加载认证配置
  if (settings.contains("auth/enable_nla")) {
    values_["nla_enabled"] = settings.value("auth/enable_nla").toBool();
    qInfo() << "Loaded auth/enable_nla:" << values_["nla_enabled"].toBool();
  }
  if (settings.contains("auth/pam_service")) {
    values_["pam_service"] = settings.value("auth/pam_service").toString();
    qInfo() << "Loaded auth/pam_service:" << values_["pam_service"].toString();
  }

  // 加载服务配置
  if (settings.contains("service/runtime_mode")) {
    values_["runtime_mode"] = settings.value("service/runtime_mode").toString();
    qInfo() << "Loaded service/runtime_mode:" << values_["runtime_mode"].toString();
  }

  qInfo() << "Configuration loaded successfully from file:" << path;
  return true;
}

bool DrdQtConfig::drd_config_merge_cli(const QVariantMap &cli_values,
                                        QString *error_message) {
  if (cli_values.contains("bind_address")) {
    values_["bind_address"] = cli_values["bind_address"];
  }
  if (cli_values.contains("port")) {
    int port = cli_values["port"].toInt();
    if (port > 0 && port <= 65535) {
      values_["port"] = port;
    } else {
      if (error_message) {
        *error_message = QString("Invalid port: %1").arg(port);
      }
      return false;
    }
  }
  if (cli_values.contains("certificate_path")) {
    QString cert_path = cli_values["certificate_path"].toString();
    if (!QFile::exists(cert_path)) {
      if (error_message) {
        *error_message = QString("Certificate file not found: %1").arg(cert_path);
      }
      return false;
    }
    values_["certificate_path"] = cert_path;
  }
  if (cli_values.contains("private_key_path")) {
    QString key_path = cli_values["private_key_path"].toString();
    if (!QFile::exists(key_path)) {
      if (error_message) {
        *error_message = QString("Private key file not found: %1").arg(key_path);
      }
      return false;
    }
    values_["private_key_path"] = key_path;
  }
  if (cli_values.contains("capture_width")) {
    values_["capture_width"] = cli_values["capture_width"].toInt();
  }
  if (cli_values.contains("capture_height")) {
    values_["capture_height"] = cli_values["capture_height"].toInt();
  }
  if (cli_values.contains("capture_target_fps")) {
    values_["capture_target_fps"] = cli_values["capture_target_fps"].toInt();
  }
  if (cli_values.contains("capture_stats_interval_sec")) {
    values_["capture_stats_interval_sec"] = cli_values["capture_stats_interval_sec"].toInt();
  }
  if (cli_values.contains("encoder_mode")) {
    QString mode = cli_values["encoder_mode"].toString().toLower();
    if (mode == "h264" || mode == "rfx" || mode == "auto") {
      values_["encoding_mode"] = mode;
    } else {
      if (error_message) {
        *error_message = QString("Invalid encoder mode: %1").arg(mode);
      }
      return false;
    }
  }
  if (cli_values.contains("nla_username")) {
    values_["nla_username"] = cli_values["nla_username"];
  }
  if (cli_values.contains("nla_password")) {
    values_["nla_password"] = cli_values["nla_password"];
  }
  if (cli_values.contains("enable_nla")) {
    values_["nla_enabled"] = true;
  }
  if (cli_values.contains("disable_nla")) {
    values_["nla_enabled"] = false;
  }
  if (cli_values.contains("runtime_mode")) {
    QString mode = cli_values["runtime_mode"].toString().toLower();
    if (mode == "user" || mode == "system" || mode == "handover") {
      values_["runtime_mode"] = mode;
    } else {
      if (error_message) {
        *error_message = QString("Invalid runtime mode: %1").arg(mode);
      }
      return false;
    }
  }
  if (cli_values.contains("enable_diff")) {
    values_["enable_frame_diff"] = true;
  }
  if (cli_values.contains("disable_diff")) {
    values_["enable_frame_diff"] = false;
  }

  // 验证必要的配置项
  QString runtime_mode = values_["runtime_mode"].toString();
  if (runtime_mode != "handover") {
    // 检查证书和密钥路径是否为空（而不是是否存在于values_中）
    QString cert_path = values_["certificate_path"].toString();
    QString key_path = values_["private_key_path"].toString();
    if (cert_path.isEmpty() || key_path.isEmpty()) {
      if (error_message) {
        *error_message = "TLS certificate and private key must be specified for non-handover modes";
      }
      return false;
    }
  }

  bool nla_enabled = values_["nla_enabled"].toBool();
  if (nla_enabled && runtime_mode != "handover") {
    // 检查NLA凭据是否为空
    QString username = values_["nla_username"].toString();
    QString password = values_["nla_password"].toString();
    if (username.isEmpty() || password.isEmpty()) {
      if (error_message) {
        *error_message = "NLA username and password must be specified for NLA-enabled modes";
      }
      return false;
    }
  }

  if (!nla_enabled && runtime_mode != "system") {
    if (error_message) {
      *error_message = "Disabling NLA requires system mode";
    }
    return false;
  }

  return true;
}

QString DrdQtConfig::drd_config_get_string(const QString &key) const {
  if (values_.contains(key)) {
    return values_[key].toString();
  }
  return QString();
}

int DrdQtConfig::drd_config_get_int(const QString &key) const {
  if (values_.contains(key)) {
    return values_[key].toInt();
  }
  return 0;
}

bool DrdQtConfig::drd_config_get_bool(const QString &key) const {
  if (values_.contains(key)) {
    return values_[key].toBool();
  }
  return false;
}
