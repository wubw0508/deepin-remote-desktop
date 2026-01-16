#pragma once

#include <QEventLoop>
#include <QObject>
#include <QString>
#include <QtGlobal>

struct DrdQtSystemDaemon {
  QObject *config = nullptr;
  QObject *runtime = nullptr;
  QObject *tls_credentials = nullptr;
};

struct DrdQtHandoverDaemon {
  QObject *config = nullptr;
  QObject *runtime = nullptr;
  QObject *tls_credentials = nullptr;
};

class DrdQtSystem : public QObject {
public:
  explicit DrdQtSystem(QObject *parent = nullptr);

  const QString &module_name() const;

  DrdQtSystemDaemon *drd_system_daemon_new(QObject *config, QObject *runtime,
                                           QObject *tls_credentials);
  bool drd_system_daemon_start(DrdQtSystemDaemon *daemon,
                               QString *error_message);
  void drd_system_daemon_stop(DrdQtSystemDaemon *daemon);
  bool drd_system_daemon_set_main_loop(DrdQtSystemDaemon *daemon,
                                       QEventLoop *loop);
  quint32
  drd_system_daemon_get_pending_client_count(DrdQtSystemDaemon *daemon) const;
  quint32
  drd_system_daemon_get_remote_client_count(DrdQtSystemDaemon *daemon) const;

  DrdQtHandoverDaemon *drd_handover_daemon_new(QObject *config,
                                               QObject *runtime,
                                               QObject *tls_credentials);
  bool drd_handover_daemon_start(DrdQtHandoverDaemon *daemon,
                                 QString *error_message);
  void drd_handover_daemon_stop(DrdQtHandoverDaemon *daemon);
  bool drd_handover_daemon_set_main_loop(DrdQtHandoverDaemon *daemon,
                                         QEventLoop *loop);

private:
  QString module_name_;
};
