#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

class DrdQtTlsCredentials;

class DrdQtServerRuntime : public QObject {
  Q_OBJECT
public:
  explicit DrdQtServerRuntime(QObject *parent = nullptr);

  bool drd_server_runtime_prepare_stream(QString *error_message);
  void drd_server_runtime_stop();

  DrdQtTlsCredentials *tlsCredentials() const;
  void setTlsCredentials(DrdQtTlsCredentials *credentials);

private:
  DrdQtTlsCredentials *tls_credentials_;
};
