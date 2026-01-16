#include "drd_encoding_manager.h"

DrdQtEncodingManager::DrdQtEncodingManager(QObject *parent) : QObject(parent) {}

bool DrdQtEncodingManager::drd_encoding_manager_start(QString *error_message) {
  if (error_message) {
    error_message->clear();
  }
  running_ = true;
  return true;
}

void DrdQtEncodingManager::drd_encoding_manager_stop() { running_ = false; }

bool DrdQtEncodingManager::drd_encoding_manager_is_running() const {
  return running_;
}
