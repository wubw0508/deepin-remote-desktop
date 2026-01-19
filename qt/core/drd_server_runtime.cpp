#include "drd_server_runtime.h"
#include "security/drd_tls_credentials.h"
#include "drd_server_runtime.moc"

DrdQtServerRuntime::DrdQtServerRuntime(QObject *parent)
    : QObject(parent)
    , tls_credentials_(nullptr) {}

bool DrdQtServerRuntime::drd_server_runtime_prepare_stream(
    QString *error_message) {
  if (error_message) {
    error_message->clear();
  }
  return true;
}

void DrdQtServerRuntime::drd_server_runtime_stop() {}

DrdQtTlsCredentials *DrdQtServerRuntime::tlsCredentials() const {
  return tls_credentials_;
}

void DrdQtServerRuntime::setTlsCredentials(DrdQtTlsCredentials *credentials) {
  tls_credentials_ = credentials;
}
