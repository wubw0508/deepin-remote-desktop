#include "drd_server_runtime.h"

DrdQtServerRuntime::DrdQtServerRuntime(QObject *parent) : QObject(parent) {}

bool DrdQtServerRuntime::drd_server_runtime_prepare_stream(
    QString *error_message) {
  if (error_message) {
    error_message->clear();
  }
  return true;
}

void DrdQtServerRuntime::drd_server_runtime_stop() {}
