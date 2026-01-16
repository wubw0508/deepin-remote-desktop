#include "qt/security/drd_security.h"

DrdQtSecurity::DrdQtSecurity(QObject *parent)
    : QObject(parent), module_name_(QStringLiteral("security")) {}

const QString &DrdQtSecurity::module_name() const { return module_name_; }

DrdQtTlsCredentials *
DrdQtSecurity::drd_tls_credentials_new(const QString &certificate_path,
                                       const QString &private_key_path,
                                       QString *error_message) {
  if (error_message) {
    error_message->clear();
  }
  auto *credentials = new DrdQtTlsCredentials();
  credentials->certificate_path = certificate_path;
  credentials->private_key_path = private_key_path;
  return credentials;
}

DrdQtTlsCredentials *DrdQtSecurity::drd_tls_credentials_new_empty() {
  return new DrdQtTlsCredentials();
}

QString DrdQtSecurity::drd_tls_credentials_get_certificate_path(
    const DrdQtTlsCredentials *credentials) const {
  if (!credentials) {
    return QString();
  }
  return credentials->certificate_path;
}

QString DrdQtSecurity::drd_tls_credentials_get_private_key_path(
    const DrdQtTlsCredentials *credentials) const {
  if (!credentials) {
    return QString();
  }
  return credentials->private_key_path;
}

bool DrdQtSecurity::drd_tls_credentials_apply(DrdQtTlsCredentials *credentials,
                                              rdpSettings *settings,
                                              QString *error_message) {
  Q_UNUSED(credentials);
  Q_UNUSED(settings);
  if (error_message) {
    error_message->clear();
  }
  return true;
}

rdpCertificate *DrdQtSecurity::drd_tls_credentials_get_certificate(
    DrdQtTlsCredentials *credentials) {
  Q_UNUSED(credentials);
  return nullptr;
}

rdpPrivateKey *DrdQtSecurity::drd_tls_credentials_get_private_key(
    DrdQtTlsCredentials *credentials) {
  Q_UNUSED(credentials);
  return nullptr;
}

bool DrdQtSecurity::drd_tls_credentials_read_material(
    DrdQtTlsCredentials *credentials, QString *certificate, QString *key,
    QString *error_message) {
  Q_UNUSED(credentials);
  if (certificate) {
    certificate->clear();
  }
  if (key) {
    key->clear();
  }
  if (error_message) {
    error_message->clear();
  }
  return false;
}

bool DrdQtSecurity::drd_tls_credentials_reload_from_pem(
    DrdQtTlsCredentials *credentials, const QString &certificate_pem,
    const QString &key_pem, QString *error_message) {
  Q_UNUSED(credentials);
  Q_UNUSED(certificate_pem);
  Q_UNUSED(key_pem);
  if (error_message) {
    error_message->clear();
  }
  return false;
}

DrdQtLocalSession *DrdQtSecurity::drd_local_session_new(
    const QString &pam_service, const QString &username, const QString &domain,
    const QString &password, const QString &remote_host,
    QString *error_message) {
  if (error_message) {
    error_message->clear();
  }
  auto *session = new DrdQtLocalSession();
  session->pam_service = pam_service;
  session->username = username;
  session->domain = domain;
  session->password = password;
  session->remote_host = remote_host;
  return session;
}

void DrdQtSecurity::drd_local_session_close(DrdQtLocalSession *session) {
  delete session;
}

DrdQtNlaSamFile *DrdQtSecurity::drd_nla_sam_file_new(const QString &username,
                                                     const QString &nt_hash_hex,
                                                     QString *error_message) {
  if (error_message) {
    error_message->clear();
  }
  auto *sam_file = new DrdQtNlaSamFile();
  sam_file->username = username;
  sam_file->nt_hash_hex = nt_hash_hex;
  return sam_file;
}

QString DrdQtSecurity::drd_nla_sam_file_get_path(
    const DrdQtNlaSamFile *sam_file) const {
  if (!sam_file) {
    return QString();
  }
  return sam_file->path;
}

void DrdQtSecurity::drd_nla_sam_file_free(DrdQtNlaSamFile *sam_file) {
  delete sam_file;
}

QString
DrdQtSecurity::drd_nla_sam_hash_password(const QString &password) const {
  Q_UNUSED(password);
  return QString();
}
