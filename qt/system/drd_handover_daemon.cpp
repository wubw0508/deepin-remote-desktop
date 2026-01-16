#include "drd_handover_daemon.h"

DrdQtHandoverDaemon::DrdQtHandoverDaemon(QObject *config, QObject *runtime,
                                         QObject *tls_credentials,
                                         QObject *parent)
    : QObject(parent), config_(config), runtime_(runtime),
      tls_credentials_(tls_credentials) {}

bool DrdQtHandoverDaemon::start(QString *error_message) {
  if (running_) {
    if (error_message) {
      error_message->clear();
    }
    return true;
  }
  if (!config_ || !runtime_ || !tls_credentials_) {
    if (error_message) {
      *error_message =
          QStringLiteral("Handover daemon missing required dependencies");
    }
    return false;
  }
  if (error_message) {
    error_message->clear();
  }
  running_ = true;
  return true;
}

void DrdQtHandoverDaemon::stop() {
  if (!running_) {
    return;
  }
  if (main_loop_) {
    main_loop_->quit();
  }
  running_ = false;
}

bool DrdQtHandoverDaemon::set_main_loop(QEventLoop *loop) {
  main_loop_ = loop;
  return true;
}

QObject *DrdQtHandoverDaemon::config() const { return config_; }

QObject *DrdQtHandoverDaemon::runtime() const { return runtime_; }

QObject *DrdQtHandoverDaemon::tls_credentials() const {
  return tls_credentials_;
}

bool DrdQtHandoverDaemon::running() const { return running_; }
