#pragma once

#include <QString>
#include <QVariantMap>

struct DrdQtEncodingOptions {
  QVariantMap options;

  QString drd_encoding_options_get_string(const QString &key) const;
  int drd_encoding_options_get_int(const QString &key) const;
  bool drd_encoding_options_get_bool(const QString &key) const;
};
