#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

struct rdpCertificate;
struct rdpPrivateKey;
struct rdpSettings;

struct DrdQtTlsCredentials {
  QString certificate_path;
  QString private_key_path;
};

struct DrdQtLocalSession {
  QString pam_service;
  QString username;
  QString domain;
  QString password;
  QString remote_host;
};

struct DrdQtNlaSamFile {
  QString username;
  QString nt_hash_hex;
  QString path;
};

class DrdQtSecurity : public QObject {
public:
  explicit DrdQtSecurity(QObject *parent = nullptr);

  const QString &module_name() const;

  DrdQtTlsCredentials *drd_tls_credentials_new(const QString &certificate_path,
                                               const QString &private_key_path,
                                               QString *error_message);
  DrdQtTlsCredentials *drd_tls_credentials_new_empty();
  QString drd_tls_credentials_get_certificate_path(
      const DrdQtTlsCredentials *credentials) const;
  QString drd_tls_credentials_get_private_key_path(
      const DrdQtTlsCredentials *credentials) const;
  bool drd_tls_credentials_apply(DrdQtTlsCredentials *credentials,
                                 rdpSettings *settings, QString *error_message);
  rdpCertificate *
  drd_tls_credentials_get_certificate(DrdQtTlsCredentials *credentials);
  rdpPrivateKey *
  drd_tls_credentials_get_private_key(DrdQtTlsCredentials *credentials);
  bool drd_tls_credentials_read_material(DrdQtTlsCredentials *credentials,
                                         QString *certificate, QString *key,
                                         QString *error_message);
  bool drd_tls_credentials_reload_from_pem(DrdQtTlsCredentials *credentials,
                                           const QString &certificate_pem,
                                           const QString &key_pem,
                                           QString *error_message);

  DrdQtLocalSession *
  drd_local_session_new(const QString &pam_service, const QString &username,
                        const QString &domain, const QString &password,
                        const QString &remote_host, QString *error_message);
  void drd_local_session_close(DrdQtLocalSession *session);

  DrdQtNlaSamFile *drd_nla_sam_file_new(const QString &username,
                                        const QString &nt_hash_hex,
                                        QString *error_message);
  QString drd_nla_sam_file_get_path(const DrdQtNlaSamFile *sam_file) const;
  void drd_nla_sam_file_free(DrdQtNlaSamFile *sam_file);
  QString drd_nla_sam_hash_password(const QString &password) const;

private:
  QString module_name_;
};
