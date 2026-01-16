#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

class DrdQtEncodingManager : public QObject {
public:
  explicit DrdQtEncodingManager(QObject *parent = nullptr);

  bool drd_encoding_manager_start(QString *error_message);
  void drd_encoding_manager_stop();
  bool drd_encoding_manager_is_running() const;

private:
  bool running_ = false;
};
