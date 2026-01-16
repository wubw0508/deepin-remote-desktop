#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

class DrdQtInputDispatcher : public QObject {
public:
  explicit DrdQtInputDispatcher(QObject *parent = nullptr);

  bool drd_input_dispatcher_start(QString *error_message);
  void drd_input_dispatcher_stop();
  bool drd_input_dispatcher_is_running() const;

private:
  bool running_ = false;
};
