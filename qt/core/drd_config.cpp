#include "drd_config.h"

DrdQtConfig::DrdQtConfig(QObject *parent) : QObject(parent) {}

bool DrdQtConfig::drd_config_load_from_file(const QString &path,
                                            QString *error_message) {
  Q_UNUSED(path);
  if (error_message) {
    error_message->clear();
  }
  return false;
}

bool DrdQtConfig::drd_config_merge_cli(const QVariantMap &cli_values,
                                       QString *error_message) {
  Q_UNUSED(cli_values);
  if (error_message) {
    error_message->clear();
  }
  return true;
}

QString DrdQtConfig::drd_config_get_string(const QString &key) const {
  Q_UNUSED(key);
  return QString();
}

int DrdQtConfig::drd_config_get_int(const QString &key) const {
  Q_UNUSED(key);
  return 0;
}

bool DrdQtConfig::drd_config_get_bool(const QString &key) const {
  Q_UNUSED(key);
  return false;
}
