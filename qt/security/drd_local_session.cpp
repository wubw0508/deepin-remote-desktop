#include "drd_local_session.h"

DrdQtLocalSession::DrdQtLocalSession(QObject *parent) : QObject(parent) {}

bool DrdQtLocalSession::drd_local_session_open(const QString &pam_service,
                                               const QString &username,
                                               const QString &domain,
                                               const QString &password,
                                               const QString &remote_host,
                                               QString *error_message) {
  pam_service_ = pam_service;
  username_ = username;
  domain_ = domain;
  password_ = password;
  remote_host_ = remote_host;
  if (error_message) {
    error_message->clear();
  }
  return true;
}

void DrdQtLocalSession::drd_local_session_close() {}
