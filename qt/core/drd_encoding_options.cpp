#include "drd_encoding_options.h"

QString DrdQtEncodingOptions::drd_encoding_options_get_string(
    const QString &key) const {
  Q_UNUSED(key);
  return QString();
}

int DrdQtEncodingOptions::drd_encoding_options_get_int(
    const QString &key) const {
  Q_UNUSED(key);
  return 0;
}

bool DrdQtEncodingOptions::drd_encoding_options_get_bool(
    const QString &key) const {
  Q_UNUSED(key);
  return false;
}
