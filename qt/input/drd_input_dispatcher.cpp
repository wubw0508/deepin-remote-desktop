#include "drd_input_dispatcher.h"

DrdQtInputDispatcher::DrdQtInputDispatcher(QObject *parent) : QObject(parent) {}

bool DrdQtInputDispatcher::drd_input_dispatcher_start(QString *error_message) {
  if (error_message) {
    error_message->clear();
  }
  running_ = true;
  return true;
}

void DrdQtInputDispatcher::drd_input_dispatcher_stop() { running_ = false; }

bool DrdQtInputDispatcher::drd_input_dispatcher_is_running() const {
  return running_;
}
