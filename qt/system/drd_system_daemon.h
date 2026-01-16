#pragma once

#include <QEventLoop>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QtGlobal>

#include "system/drd_handover_daemon.h"

class DrdQtSystemDaemon : public QObject {
public:
  explicit DrdQtSystemDaemon(QObject *config, QObject *runtime,
                             QObject *tls_credentials,
                             QObject *parent = nullptr);

  bool start(QString *error_message);
  void stop();
  bool set_main_loop(QEventLoop *loop);
  quint32 pending_client_count() const;
  quint32 remote_client_count() const;

  QObject *config() const;
  QObject *runtime() const;
  QObject *tls_credentials() const;
  DrdQtHandoverDaemon *handover_daemon() const;
  bool running() const;

private:
  QPointer<QObject> config_;
  QPointer<QObject> runtime_;
  QPointer<QObject> tls_credentials_;
  QPointer<QEventLoop> main_loop_;
  quint32 pending_client_count_ = 0;
  quint32 remote_client_count_ = 0;
  bool running_ = false;
  DrdQtHandoverDaemon *handover_daemon_ = nullptr;
};
