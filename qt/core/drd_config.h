#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class DrdQtConfig : public QObject {
public:
  explicit DrdQtConfig(QObject *parent = nullptr);

  bool drd_config_load_from_file(const QString &path, QString *error_message);
  bool drd_config_merge_cli(const QVariantMap &cli_values,
                            QString *error_message);
  QString drd_config_get_string(const QString &key) const;
  int drd_config_get_int(const QString &key) const;
  bool drd_config_get_bool(const QString &key) const;

private:
  QVariantMap values_;
};
