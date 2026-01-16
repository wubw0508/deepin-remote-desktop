#pragma once

#include <QEventLoop>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QtGlobal>

class DrdQtHandoverDaemon : public QObject {
public:
  explicit DrdQtHandoverDaemon(QObject *config, QObject *runtime,
                               QObject *tls_credentials,
                               QObject *parent = nullptr);

  bool start(QString *error_message);
  void stop();
  bool set_main_loop(QEventLoop *loop);

  QObject *config() const;
  QObject *runtime() const;
  QObject *tls_credentials() const;
  bool running() const;

private:
  QPointer<QObject> config_;
  QPointer<QObject> runtime_;
  QPointer<QObject> tls_credentials_;
  QPointer<QEventLoop> main_loop_;
  bool running_ = false;
};
