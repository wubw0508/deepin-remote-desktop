#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

class DrdQtServerRuntime : public QObject {
public:
  explicit DrdQtServerRuntime(QObject *parent = nullptr);

  bool drd_server_runtime_prepare_stream(QString *error_message);
  void drd_server_runtime_stop();
};
