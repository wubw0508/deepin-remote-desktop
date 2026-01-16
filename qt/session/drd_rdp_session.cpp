#include "drd_rdp_session.h"

DrdQtRdpSession::DrdQtRdpSession(QObject *parent) : QObject(parent) {}

bool DrdQtRdpSession::drd_rdp_session_start(QString *error_message) {
  if (error_message) {
    error_message->clear();
  }
  running_ = true;
  return true;
}

void DrdQtRdpSession::drd_rdp_session_stop() { running_ = false; }

bool DrdQtRdpSession::drd_rdp_session_is_running() const { return running_; }
