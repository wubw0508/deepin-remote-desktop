#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSharedPointer>

class DrdQtConfig;
class DrdQtServerRuntime;
class DrdQtTlsCredentials;
class DrdQtRdpListener;
class DrdQtSystemDaemon;
class DrdQtHandoverDaemon;

class DrdQtApplication : public QObject {
public:
  explicit DrdQtApplication(QObject *parent = nullptr);

  int drd_application_run(const QStringList &arguments, QString *error_message);

private:
  QSharedPointer<DrdQtConfig> config_;
  QSharedPointer<DrdQtServerRuntime> runtime_;
  QSharedPointer<DrdQtTlsCredentials> tls_credentials_;
  QSharedPointer<DrdQtRdpListener> listener_;
  QSharedPointer<DrdQtSystemDaemon> system_daemon_;
  QSharedPointer<DrdQtHandoverDaemon> handover_daemon_;
};
