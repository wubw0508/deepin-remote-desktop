#include "drd_tls_credentials.h"
#include "drd_tls_credentials.moc"

#include <freerdp/crypto/certificate.h>
#include <freerdp/crypto/privatekey.h>
#include <freerdp/settings.h>

#include <QFile>
#include <QIODevice>

DrdQtTlsCredentials::DrdQtTlsCredentials(QObject *parent) : QObject(parent) {}

bool DrdQtTlsCredentials::apply(rdpSettings *settings, QString *error_message) {
  if (!settings) {
    if (error_message) {
      *error_message = QStringLiteral("Invalid settings");
    }
    return false;
  }

  rdpCertificate* certificate = nullptr;
  rdpPrivateKey* key = nullptr;

  // 优先使用 PEM 数据
  if (!certificate_pem_.isEmpty() && !private_key_pem_.isEmpty()) {
    certificate = freerdp_certificate_new_from_pem(certificate_pem_.toUtf8().constData());
    if (certificate == nullptr) {
      if (error_message) {
        *error_message = QStringLiteral("Failed to parse TLS certificate material (PEM)");
      }
      return false;
    }

    key = freerdp_key_new_from_pem(private_key_pem_.toUtf8().constData());
    if (key == nullptr) {
      freerdp_certificate_free(certificate);
      if (error_message) {
        *error_message = QStringLiteral("Failed to parse TLS private key material (PEM)");
      }
      return false;
    }
  } else if (!certificate_path_.isEmpty() && !private_key_path_.isEmpty()) {
    // 回退到文件路径
    QByteArray cert_data;
    QByteArray key_data;

    QFile cert_file(certificate_path_);
    if (!cert_file.open(QIODevice::ReadOnly)) {
      if (error_message) {
        *error_message = QStringLiteral("Failed to read certificate file: %1").arg(cert_file.errorString());
      }
      return false;
    }
    cert_data = cert_file.readAll();
    cert_file.close();

    QFile key_file(private_key_path_);
    if (!key_file.open(QIODevice::ReadOnly)) {
      if (error_message) {
        *error_message = QStringLiteral("Failed to read private key file: %1").arg(key_file.errorString());
      }
      return false;
    }
    key_data = key_file.readAll();
    key_file.close();

    certificate = freerdp_certificate_new_from_pem(cert_data.constData());
    if (certificate == nullptr) {
      if (error_message) {
        *error_message = QStringLiteral("Failed to parse TLS certificate material (file)");
      }
      return false;
    }

    key = freerdp_key_new_from_pem(key_data.constData());
    if (key == nullptr) {
      freerdp_certificate_free(certificate);
      if (error_message) {
        *error_message = QStringLiteral("Failed to parse TLS private key material (file)");
      }
      return false;
    }
  } else {
    if (error_message) {
      *error_message = QStringLiteral("No TLS credentials available");
    }
    return false;
  }

  if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerCertificate, certificate, 1)) {
    freerdp_certificate_free(certificate);
    if (key) {
      freerdp_key_free(key);
    }
    if (error_message) {
      *error_message = QStringLiteral("Failed to assign server certificate to settings");
    }
    return false;
  }

  if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerRsaKey, key, 1)) {
    freerdp_key_free(key);
    if (error_message) {
      *error_message = QStringLiteral("Failed to assign private key to settings");
    }
    return false;
  }

  return true;
}

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
