#pragma once

#include <QIODevice>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <QtGlobal>
#include <functional>

class QTcpServer;

struct DrdQtRoutingTokenInfo {
  bool requested_rdstls = false;
  QString routing_token;
};

class DrdQtRdpListener : public QObject {
  Q_OBJECT
public:
  using ListenerDelegate =
      std::function<bool(QObject *, QIODevice *, QObject *, QString *)>;
  using ListenerSessionCallback =
      std::function<void(QObject *, QObject *, QIODevice *, QObject *)>;

  DrdQtRdpListener(const QString &bind_address, quint16 port, QObject *runtime,
                   const QVariantMap &encoding_options, bool nla_enabled,
                   const QString &nla_username, const QString &nla_password,
                   const QString &pam_service, const QString &runtime_mode_name,
                   QObject *parent);

  bool start(QString *error_message);
  void stop();
  QObject *runtime() const;
  void set_delegate(const ListenerDelegate &func, QObject *user_data);
  void set_session_callback(const ListenerSessionCallback &func,
                            QObject *user_data);
  bool adopt_connection(QIODevice *connection, QString *error_message);
  bool is_handover_mode() const;

private:
  QString bind_address_;
  quint16 port_ = 0;
  QPointer<QObject> runtime_;
  QVariantMap encoding_options_;
  bool nla_enabled_ = false;
  QString nla_username_;
  QString nla_password_;
  QString pam_service_;
  QString runtime_mode_name_;
  bool running_ = false;
  ListenerDelegate delegate_func_;
  QPointer<QObject> delegate_user_data_;
  ListenerSessionCallback session_cb_;
  QPointer<QObject> session_cb_data_;
  QList<QPointer<QObject>> sessions_;
  QPointer<QTcpServer> server_;
};

class DrdQtTransport : public QObject {
public:
  using ListenerDelegate =
      std::function<bool(QObject *, QIODevice *, QObject *, QString *)>;
  using ListenerSessionCallback =
      std::function<void(QObject *, QObject *, QIODevice *, QObject *)>;

  explicit DrdQtTransport(QObject *parent = nullptr);

  const QString &module_name() const;

  QObject *drd_rdp_listener_new(const QString &bind_address, quint16 port,
                                QObject *runtime,
                                const QVariantMap &encoding_options,
                                bool nla_enabled, const QString &nla_username,
                                const QString &nla_password,
                                const QString &pam_service,
                                const QString &runtime_mode_name);
  bool drd_rdp_listener_start(QObject *listener, QString *error_message);
  void drd_rdp_listener_stop(QObject *listener);
  QObject *drd_rdp_listener_get_runtime(QObject *listener) const;
  void drd_rdp_listener_set_delegate(QObject *listener,
                                     const ListenerDelegate &func,
                                     QObject *user_data);
  bool drd_rdp_listener_adopt_connection(QObject *listener,
                                         QIODevice *connection,
                                         QString *error_message);
  void
  drd_rdp_listener_set_session_callback(QObject *listener,
                                        const ListenerSessionCallback &func,
                                        QObject *user_data);
  bool drd_rdp_listener_is_handover_mode(QObject *listener) const;

  DrdQtRoutingTokenInfo *drd_routing_token_info_new();
  void drd_routing_token_info_free(DrdQtRoutingTokenInfo *info);
  bool drd_routing_token_peek(QIODevice *connection, QObject *cancellable,
                              DrdQtRoutingTokenInfo *info,
                              QString *error_message);

private:
  QString module_name_;
};
