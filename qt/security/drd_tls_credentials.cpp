#include "drd_tls_credentials.h"

DrdQtTlsCredentials::DrdQtTlsCredentials(QObject *parent) : QObject(parent) {}

bool DrdQtTlsCredentials::drd_tls_credentials_load(
    const QString &certificate_path, const QString &private_key_path,
    QString *error_message) {
  certificate_path_ = certificate_path;
  private_key_path_ = private_key_path;
  certificate_pem_.clear();
  private_key_pem_.clear();
  if (error_message) {
    error_message->clear();
  }
  return true;
}

bool DrdQtTlsCredentials::drd_tls_credentials_reload_from_pem(
    const QString &certificate_pem, const QString &private_key_pem,
    QString *error_message) {
  if (certificate_pem.isEmpty() || private_key_pem.isEmpty()) {
    if (error_message) {
      *error_message =
          QStringLiteral("TLS credentials PEM material is incomplete");
    }
    return false;
  }
  certificate_pem_ = certificate_pem;
  private_key_pem_ = private_key_pem;
  certificate_path_.clear();
  private_key_path_.clear();
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

QString DrdQtTlsCredentials::drd_tls_credentials_certificate_pem() const {
  return certificate_pem_;
}

QString DrdQtTlsCredentials::drd_tls_credentials_private_key_pem() const {
  return private_key_pem_;
}
