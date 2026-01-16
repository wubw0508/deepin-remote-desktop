#include "drd_transport.h"

DrdQtTransport::DrdQtTransport(QObject *parent)
    : QObject(parent), module_name_(QStringLiteral("transport")) {}

const QString &DrdQtTransport::module_name() const { return module_name_; }

QObject *DrdQtTransport::drd_rdp_listener_new(
    const QString &bind_address, quint16 port, QObject *runtime,
    const QVariantMap &encoding_options, bool nla_enabled,
    const QString &nla_username, const QString &nla_password,
    const QString &pam_service, const QString &runtime_mode_name) {
  Q_UNUSED(bind_address);
  Q_UNUSED(port);
  Q_UNUSED(runtime);
  Q_UNUSED(encoding_options);
  Q_UNUSED(nla_enabled);
  Q_UNUSED(nla_username);
  Q_UNUSED(nla_password);
  Q_UNUSED(pam_service);
  Q_UNUSED(runtime_mode_name);
  return new QObject(this);
}

bool DrdQtTransport::drd_rdp_listener_start(QObject *listener,
                                            QString *error_message) {
  Q_UNUSED(listener);
  if (error_message) {
    error_message->clear();
  }
  return true;
}

void DrdQtTransport::drd_rdp_listener_stop(QObject *listener) {
  Q_UNUSED(listener);
}

QObject *DrdQtTransport::drd_rdp_listener_get_runtime(QObject *listener) const {
  Q_UNUSED(listener);
  return nullptr;
}

void DrdQtTransport::drd_rdp_listener_set_delegate(QObject *listener,
                                                   const ListenerDelegate &func,
                                                   QObject *user_data) {
  Q_UNUSED(listener);
  Q_UNUSED(func);
  Q_UNUSED(user_data);
}

bool DrdQtTransport::drd_rdp_listener_adopt_connection(QObject *listener,
                                                       QIODevice *connection,
                                                       QString *error_message) {
  Q_UNUSED(listener);
  Q_UNUSED(connection);
  if (error_message) {
    error_message->clear();
  }
  return true;
}

void DrdQtTransport::drd_rdp_listener_set_session_callback(
    QObject *listener, const ListenerSessionCallback &func,
    QObject *user_data) {
  Q_UNUSED(listener);
  Q_UNUSED(func);
  Q_UNUSED(user_data);
}

bool DrdQtTransport::drd_rdp_listener_is_handover_mode(
    QObject *listener) const {
  Q_UNUSED(listener);
  return false;
}

DrdQtRoutingTokenInfo *DrdQtTransport::drd_routing_token_info_new() {
  return new DrdQtRoutingTokenInfo();
}

void DrdQtTransport::drd_routing_token_info_free(DrdQtRoutingTokenInfo *info) {
  delete info;
}

bool DrdQtTransport::drd_routing_token_peek(QIODevice *connection,
                                            QObject *cancellable,
                                            DrdQtRoutingTokenInfo *info,
                                            QString *error_message) {
  Q_UNUSED(connection);
  Q_UNUSED(cancellable);
  Q_UNUSED(info);
  if (error_message) {
    error_message->clear();
  }
  return false;
}
