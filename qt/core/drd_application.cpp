#include "drd_application.h"

DrdQtApplication::DrdQtApplication(QObject *parent) : QObject(parent) {}

int DrdQtApplication::drd_application_run(const QStringList &arguments,
                                          QString *error_message) {
  Q_UNUSED(arguments);
  if (error_message) {
    error_message->clear();
  }
  return 0;
}
