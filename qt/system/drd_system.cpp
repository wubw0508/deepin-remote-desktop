#include "drd_system.h"

DrdQtSystem::DrdQtSystem(QObject *parent)
    : QObject(parent), module_name_(QStringLiteral("system")) {}

const QString &DrdQtSystem::module_name() const { return module_name_; }

DrdQtSystemDaemon *
DrdQtSystem::drd_system_daemon_new(QObject *config, QObject *runtime,
                                   QObject *tls_credentials) {
  auto *daemon = new DrdQtSystemDaemon();
  daemon->config = config;
  daemon->runtime = runtime;
  daemon->tls_credentials = tls_credentials;
  return daemon;
}

bool DrdQtSystem::drd_system_daemon_start(DrdQtSystemDaemon *daemon,
                                          QString *error_message) {
  Q_UNUSED(daemon);
  if (error_message) {
    error_message->clear();
  }
  return true;
}

void DrdQtSystem::drd_system_daemon_stop(DrdQtSystemDaemon *daemon) {
  delete daemon;
}

bool DrdQtSystem::drd_system_daemon_set_main_loop(DrdQtSystemDaemon *daemon,
                                                  QEventLoop *loop) {
  Q_UNUSED(daemon);
  Q_UNUSED(loop);
  return true;
}

quint32 DrdQtSystem::drd_system_daemon_get_pending_client_count(
    DrdQtSystemDaemon *daemon) const {
  Q_UNUSED(daemon);
  return 0;
}

quint32 DrdQtSystem::drd_system_daemon_get_remote_client_count(
    DrdQtSystemDaemon *daemon) const {
  Q_UNUSED(daemon);
  return 0;
}

DrdQtHandoverDaemon *
DrdQtSystem::drd_handover_daemon_new(QObject *config, QObject *runtime,
                                     QObject *tls_credentials) {
  auto *daemon = new DrdQtHandoverDaemon();
  daemon->config = config;
  daemon->runtime = runtime;
  daemon->tls_credentials = tls_credentials;
  return daemon;
}

bool DrdQtSystem::drd_handover_daemon_start(DrdQtHandoverDaemon *daemon,
                                            QString *error_message) {
  Q_UNUSED(daemon);
  if (error_message) {
    error_message->clear();
  }
  return true;
}

void DrdQtSystem::drd_handover_daemon_stop(DrdQtHandoverDaemon *daemon) {
  delete daemon;
}

bool DrdQtSystem::drd_handover_daemon_set_main_loop(DrdQtHandoverDaemon *daemon,
                                                    QEventLoop *loop) {
  Q_UNUSED(daemon);
  Q_UNUSED(loop);
  return true;
}
