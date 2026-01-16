#include "drd_tls_credentials.h"

DrdQtTlsCredentials::DrdQtTlsCredentials(QObject *parent) : QObject(parent) {}

bool DrdQtTlsCredentials::drd_tls_credentials_load(
    const QString &certificate_path, const QString &private_key_path,
    QString *error_message) {
  certificate_path_ = certificate_path;
  private_key_path_ = private_key_path;
  if (error_message) {
    error_message->clear();
  }
  return true;
}

QString DrdQtTlsCredentials::drd_tls_credentials_certificate_path() const {
  return certificate_path_;
}

QString DrdQtTlsCredentials::drd_tls_credentials_private_key_path() const {
  return private_key_path_;
}
