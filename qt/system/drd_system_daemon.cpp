#include "drd_system_daemon.h"

DrdQtSystemDaemon::DrdQtSystemDaemon(QObject *config, QObject *runtime,
                                     QObject *tls_credentials, QObject *parent)
    : QObject(parent), config_(config), runtime_(runtime),
      tls_credentials_(tls_credentials),
      handover_daemon_(
          new DrdQtHandoverDaemon(config, runtime, tls_credentials, this)) {}

bool DrdQtSystemDaemon::start(QString *error_message) {
  if (running_) {
    if (error_message) {
      error_message->clear();
    }
    return true;
  }
  if (!config_ || !runtime_ || !tls_credentials_) {
    if (error_message) {
      *error_message =
          QStringLiteral("System daemon missing required dependencies");
    }
    return false;
  }
  if (handover_daemon_ && !handover_daemon_->start(error_message)) {
    return false;
  }
  if (error_message) {
    error_message->clear();
  }
  pending_client_count_ = 0;
  remote_client_count_ = 0;
  running_ = true;
  return true;
}

void DrdQtSystemDaemon::stop() {
  if (!running_) {
    return;
  }
  if (handover_daemon_) {
    handover_daemon_->stop();
  }
  if (main_loop_) {
    main_loop_->quit();
  }
  pending_client_count_ = 0;
  remote_client_count_ = 0;
  running_ = false;
}

bool DrdQtSystemDaemon::set_main_loop(QEventLoop *loop) {
  main_loop_ = loop;
  if (handover_daemon_) {
    handover_daemon_->set_main_loop(loop);
  }
  return true;
}

quint32 DrdQtSystemDaemon::pending_client_count() const {
  return pending_client_count_;
}

quint32 DrdQtSystemDaemon::remote_client_count() const {
  return remote_client_count_;
}

QObject *DrdQtSystemDaemon::config() const { return config_; }

QObject *DrdQtSystemDaemon::runtime() const { return runtime_; }

QObject *DrdQtSystemDaemon::tls_credentials() const { return tls_credentials_; }

DrdQtHandoverDaemon *DrdQtSystemDaemon::handover_daemon() const {
  return handover_daemon_;
}

bool DrdQtSystemDaemon::running() const { return running_; }
