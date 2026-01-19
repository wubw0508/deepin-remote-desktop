#pragma once

#include <QObject>
#include <QString>

class DrdQtTlsCredentials : public QObject {
  Q_OBJECT

public:
  explicit DrdQtTlsCredentials(QObject *parent = nullptr);

  bool drd_tls_credentials_load(const QString &certificate_path,
                                const QString &private_key_path,
                                QString *error_message);
  bool drd_tls_credentials_reload_from_pem(const QString &certificate_pem,
                                           const QString &private_key_pem,
                                           QString *error_message);
  QString drd_tls_credentials_certificate_path() const;
  QString drd_tls_credentials_private_key_path() const;
  QString drd_tls_credentials_certificate_pem() const;
  QString drd_tls_credentials_private_key_pem() const;

private:
  QString certificate_path_;
  QString private_key_path_;
  QString certificate_pem_;
  QString private_key_pem_;
};
