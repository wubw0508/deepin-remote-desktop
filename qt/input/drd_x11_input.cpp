#include "drd_x11_input.h"

DrdQtX11Input::DrdQtX11Input(QObject *parent) : QObject(parent) {}

bool DrdQtX11Input::drd_x11_input_start(QString *error_message) {
  if (error_message) {
    error_message->clear();
  }
  running_ = true;
  return true;
}

void DrdQtX11Input::drd_x11_input_stop() { running_ = false; }

bool DrdQtX11Input::drd_x11_input_is_running() const { return running_; }
