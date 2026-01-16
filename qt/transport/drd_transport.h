#pragma once

#include <QIODevice>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtGlobal>
#include <functional>

struct DrdQtRoutingTokenInfo {
  bool requested_rdstls = false;
  QString routing_token;
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
