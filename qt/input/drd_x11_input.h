#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

class DrdQtX11Input : public QObject {
public:
  explicit DrdQtX11Input(QObject *parent = nullptr);

  bool drd_x11_input_start(QString *error_message);
  void drd_x11_input_stop();
  bool drd_x11_input_is_running() const;

private:
  bool running_ = false;
};
