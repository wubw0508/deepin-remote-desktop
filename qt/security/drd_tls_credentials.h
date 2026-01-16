#pragma once

#include <QObject>
#include <QString>

class DrdQtTlsCredentials : public QObject {
public:
  explicit DrdQtTlsCredentials(QObject *parent = nullptr);

  bool drd_tls_credentials_load(const QString &certificate_path,
                                const QString &private_key_path,
                                QString *error_message);
  QString drd_tls_credentials_certificate_path() const;
  QString drd_tls_credentials_private_key_path() const;

private:
  QString certificate_path_;
  QString private_key_path_;
};
