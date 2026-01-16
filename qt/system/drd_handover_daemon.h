#pragma once

#include <QEventLoop>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QtGlobal>

class DrdQtTransport;
class QDBusInterface;

class DrdQtHandoverDaemon : public QObject {
public:
  explicit DrdQtHandoverDaemon(QObject *config, QObject *runtime,
                               QObject *tls_credentials,
                               QObject *parent = nullptr);

  bool start(QString *error_message);
  void stop();
  bool set_main_loop(QEventLoop *loop);
  void handle_redirect_client(const QString &routing_token,
                              const QString &username,
                              const QString &password);
  void handle_take_client_ready(bool use_system_credentials);
  void handle_restart_handover();

  QObject *config() const;
  QObject *runtime() const;
  QObject *tls_credentials() const;
  bool running() const;

private:
  bool take_client(QString *error_message);
  bool redirect_active_client(const QString &routing_token,
                              const QString &username,
                              const QString &password);
  void request_shutdown();
  void clear_nla_credentials();
  bool refresh_nla_credentials(QString *error_message);

  QPointer<QObject> config_;
  QPointer<QObject> runtime_;
  QPointer<QObject> tls_credentials_;
  QPointer<QEventLoop> main_loop_;
  QPointer<QDBusInterface> dispatcher_proxy_;
  QPointer<QDBusInterface> handover_proxy_;
  QPointer<QObject> listener_;
  QPointer<QObject> active_session_;
  QString handover_object_path_;
  QString nla_username_;
  QString nla_password_;
  DrdQtTransport *transport_ = nullptr;
  bool running_ = false;
};
