#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

class DrdQtRdpSession : public QObject {
public:
  explicit DrdQtRdpSession(QObject *parent = nullptr);

  bool drd_rdp_session_start(QString *error_message);
  void drd_rdp_session_stop();
  bool drd_rdp_session_is_running() const;

private:
  bool running_ = false;
};
