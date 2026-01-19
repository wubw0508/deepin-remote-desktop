#include "drd_transport.h"

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>

#include "session/drd_rdp_session.h"

namespace {

constexpr quint8 kTpktVersion = 3;
constexpr quint32 kProtocolRdstls = 0x00000004;
constexpr int kMinTpktLength = 4 + 7;
constexpr char kRoutingTokenPrefix[] = "Cookie: msts=";

bool runtime_mode_matches(const QString &runtime_mode_name,
                          const char *expected) {
  return runtime_mode_name.compare(QLatin1String(expected),
                                   Qt::CaseInsensitive) == 0;
}

int find_crlf(const QByteArray &buffer, int start) {
  for (int i = start; i + 1 < buffer.size(); ++i) {
    if (buffer.at(i) == '\r' && buffer.at(i + 1) == '\n') {
      return i;
    }
  }
  return -1;
}

QString read_routing_token(const QByteArray &buffer, int start, int *line_end) {
  const QByteArray prefix = QByteArrayLiteral(kRoutingTokenPrefix);
  if (buffer.size() - start < prefix.size()) {
    return QString();
  }
  if (buffer.mid(start, prefix.size()) != prefix) {
    return QString();
  }
  int end = find_crlf(buffer, start);
  if (end < 0) {
    return QString();
  }
  *line_end = end;
  const int token_start = start + prefix.size();
  return QString::fromLatin1(buffer.mid(token_start, end - token_start));
}

} // namespace

DrdQtRdpListener::DrdQtRdpListener(
    const QString &bind_address, quint16 port, QObject *runtime,
    const QVariantMap &encoding_options, bool nla_enabled,
    const QString &nla_username, const QString &nla_password,
    const QString &pam_service, const QString &runtime_mode_name,
    QObject *parent)
    : QObject(parent), bind_address_(bind_address), port_(port),
      runtime_(runtime), encoding_options_(encoding_options),
      nla_enabled_(nla_enabled), nla_username_(nla_username),
      nla_password_(nla_password), pam_service_(pam_service),
      runtime_mode_name_(runtime_mode_name) {}

bool DrdQtRdpListener::start(QString *error_message) {
  if (running_) {
    if (error_message) {
      error_message->clear();
    }
    return true;
  }
  if (!runtime_) {
    if (error_message) {
      *error_message = QStringLiteral("Listener runtime is required");
    }
    return false;
  }
  if (pam_service_.isEmpty()) {
    if (error_message) {
      *error_message = QStringLiteral("PAM service is required");
    }
    return false;
  }
  if (nla_enabled_ && (nla_username_.isEmpty() || nla_password_.isEmpty())) {
    if (error_message) {
      *error_message = QStringLiteral("NLA credentials are required");
    }
    return false;
  }
  if (!runtime_mode_matches(runtime_mode_name_, "user") &&
      !runtime_mode_matches(runtime_mode_name_, "system") &&
      !runtime_mode_matches(runtime_mode_name_, "handover")) {
    if (error_message) {
      *error_message = QStringLiteral("Invalid runtime mode");
    }
    return false;
  }
  if (!server_) {
    server_ = new QTcpServer(this);
    QObject::connect(server_, &QTcpServer::newConnection, this, [this]() {
      while (server_ && server_->hasPendingConnections()) {
        QTcpSocket *socket = server_->nextPendingConnection();
        if (!socket) {
          continue;
        }
        QString accept_error;
        if (!adopt_connection(socket, &accept_error)) {
          socket->close();
          socket->deleteLater();
        }
      }
    });
  }
  QHostAddress address;
  if (!address.setAddress(bind_address_)) {
    if (error_message) {
      *error_message = QStringLiteral("Invalid bind address");
    }
    return false;
  }
  if (!server_->listen(address, port_)) {
    if (error_message) {
      *error_message = server_->errorString();
    }
    return false;
  }
  running_ = true;
  if (error_message) {
    error_message->clear();
  }
  return true;
}

void DrdQtRdpListener::stop() {
  if (server_) {
    server_->close();
    server_->deleteLater();
    server_.clear();
  }
  running_ = false;
}

QObject *DrdQtRdpListener::runtime() const { return runtime_; }

void DrdQtRdpListener::set_delegate(const ListenerDelegate &func,
                                    QObject *user_data) {
  delegate_func_ = func;
  delegate_user_data_ = user_data;
}

void DrdQtRdpListener::set_session_callback(const ListenerSessionCallback &func,
                                            QObject *user_data) {
  session_cb_ = func;
  session_cb_data_ = user_data;
}

bool DrdQtRdpListener::adopt_connection(QIODevice *connection,
                                        QString *error_message) {
  if (!connection) {
    if (error_message) {
      *error_message = QStringLiteral("Connection is required");
    }
    return false;
  }
  if (delegate_func_) {
    QString delegate_error;
    if (delegate_func_(this, connection, delegate_user_data_,
                       &delegate_error)) {
      if (error_message) {
        *error_message = delegate_error;
      }
      return true;
    }
  }
  // TODO: Create FreeRDP peer and initialize RDP session
  // For now, create a basic session without FreeRDP peer
  auto *session = new DrdQtRdpSession(nullptr, this);
  sessions_.append(session);
  QObject::connect(session, &QObject::destroyed, this,
                   [this, session]() { sessions_.removeAll(session); });
  if (session_cb_) {
    session_cb_(this, session, connection, session_cb_data_);
  }
  if (error_message) {
    error_message->clear();
  }
  return true;
}

bool DrdQtRdpListener::is_handover_mode() const {
  return runtime_mode_matches(runtime_mode_name_, "handover");
}

DrdQtTransport::DrdQtTransport(QObject *parent)
    : QObject(parent), module_name_(QStringLiteral("transport")) {}

const QString &DrdQtTransport::module_name() const { return module_name_; }

QObject *DrdQtTransport::drd_rdp_listener_new(
    const QString &bind_address, quint16 port, QObject *runtime,
    const QVariantMap &encoding_options, bool nla_enabled,
    const QString &nla_username, const QString &nla_password,
    const QString &pam_service, const QString &runtime_mode_name) {
  QString effective_bind = bind_address;
  if (effective_bind.isEmpty()) {
    effective_bind = QStringLiteral("0.0.0.0");
  }
  if (!runtime || pam_service.isEmpty()) {
    return nullptr;
  }
  if (nla_enabled && (nla_username.isEmpty() || nla_password.isEmpty())) {
    return nullptr;
  }
  if (!runtime_mode_matches(runtime_mode_name, "user") &&
      !runtime_mode_matches(runtime_mode_name, "system") &&
      !runtime_mode_matches(runtime_mode_name, "handover")) {
    return nullptr;
  }
  return new DrdQtRdpListener(effective_bind, port, runtime, encoding_options,
                              nla_enabled, nla_username, nla_password,
                              pam_service, runtime_mode_name, this);
}

bool DrdQtTransport::drd_rdp_listener_start(QObject *listener,
                                            QString *error_message) {
  auto *qt_listener = qobject_cast<DrdQtRdpListener *>(listener);
  if (!qt_listener) {
    if (error_message) {
      *error_message = QStringLiteral("Invalid listener");
    }
    return false;
  }
  return qt_listener->start(error_message);
}

void DrdQtTransport::drd_rdp_listener_stop(QObject *listener) {
  auto *qt_listener = qobject_cast<DrdQtRdpListener *>(listener);
  if (!qt_listener) {
    return;
  }
  qt_listener->stop();
}

QObject *DrdQtTransport::drd_rdp_listener_get_runtime(QObject *listener) const {
  auto *qt_listener = qobject_cast<DrdQtRdpListener *>(listener);
  if (!qt_listener) {
    return nullptr;
  }
  return qt_listener->runtime();
}

void DrdQtTransport::drd_rdp_listener_set_delegate(QObject *listener,
                                                   const ListenerDelegate &func,
                                                   QObject *user_data) {
  auto *qt_listener = qobject_cast<DrdQtRdpListener *>(listener);
  if (!qt_listener) {
    return;
  }
  qt_listener->set_delegate(func, user_data);
}

bool DrdQtTransport::drd_rdp_listener_adopt_connection(QObject *listener,
                                                       QIODevice *connection,
                                                       QString *error_message) {
  auto *qt_listener = qobject_cast<DrdQtRdpListener *>(listener);
  if (!qt_listener) {
    if (error_message) {
      *error_message = QStringLiteral("Invalid listener");
    }
    return false;
  }
  return qt_listener->adopt_connection(connection, error_message);
}

void DrdQtTransport::drd_rdp_listener_set_session_callback(
    QObject *listener, const ListenerSessionCallback &func,
    QObject *user_data) {
  auto *qt_listener = qobject_cast<DrdQtRdpListener *>(listener);
  if (!qt_listener) {
    return;
  }
  qt_listener->set_session_callback(func, user_data);
}

bool DrdQtTransport::drd_rdp_listener_is_handover_mode(
    QObject *listener) const {
  auto *qt_listener = qobject_cast<DrdQtRdpListener *>(listener);
  if (!qt_listener) {
    return false;
  }
  return qt_listener->is_handover_mode();
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
  Q_UNUSED(cancellable);
  if (!connection || !info) {
    if (error_message) {
      *error_message = QStringLiteral("Connection and info are required");
    }
    return false;
  }
  if (!connection->isOpen()) {
    if (error_message) {
      *error_message = QStringLiteral("Connection is not open");
    }
    return false;
  }
  const QByteArray header = connection->peek(4);
  if (header.size() < 4) {
    if (error_message) {
      *error_message = QStringLiteral("TPKT header is incomplete");
    }
    return false;
  }
  const quint8 version = static_cast<quint8>(header.at(0));
  const quint16 tpkt_length = (static_cast<quint16>(header.at(2)) << 8) |
                              static_cast<quint8>(header.at(3));
  if (version != kTpktVersion) {
    if (error_message) {
      *error_message = QStringLiteral("The TPKT header doesn't have version 3");
    }
    return false;
  }
  if (tpkt_length < kMinTpktLength) {
    if (error_message) {
      *error_message = QStringLiteral("The x224Crq TPDU length is too short");
    }
    return false;
  }
  const QByteArray pdu = connection->peek(tpkt_length);
  if (pdu.size() < tpkt_length) {
    if (error_message) {
      *error_message = QStringLiteral("TPKT payload is incomplete");
    }
    return false;
  }

  const int x224_offset = 4;
  const quint8 length_indicator = static_cast<quint8>(pdu.at(x224_offset));
  const quint8 cr_cdt = static_cast<quint8>(pdu.at(x224_offset + 1));
  const quint16 dst_ref = (static_cast<quint16>(pdu.at(x224_offset + 2)) << 8) |
                          static_cast<quint8>(pdu.at(x224_offset + 3));
  const quint8 class_opt = static_cast<quint8>(pdu.at(x224_offset + 6));
  if (tpkt_length - 5 != length_indicator || cr_cdt != 0xE0 || dst_ref != 0 ||
      (class_opt & 0xFC) != 0) {
    if (error_message) {
      *error_message = QStringLiteral("Wrong info on x224Crq");
    }
    return false;
  }

  info->routing_token.clear();
  info->requested_rdstls = false;
  int token_line_end = -1;
  const int cookie_offset = x224_offset + 7;
  const QString token = read_routing_token(pdu, cookie_offset, &token_line_end);
  if (token.isEmpty()) {
    if (error_message) {
      error_message->clear();
    }
    return true;
  }
  info->routing_token = token;

  const int after_cookie = token_line_end + 2;
  if (after_cookie + 8 > pdu.size()) {
    if (error_message) {
      error_message->clear();
    }
    return true;
  }
  const quint8 neg_type = static_cast<quint8>(pdu.at(after_cookie));
  const quint16 neg_length =
      static_cast<quint16>(pdu.at(after_cookie + 2)) |
      (static_cast<quint16>(pdu.at(after_cookie + 3)) << 8);
  const quint32 requested_protocols =
      static_cast<quint32>(static_cast<quint8>(pdu.at(after_cookie + 4))) |
      (static_cast<quint32>(static_cast<quint8>(pdu.at(after_cookie + 5)))
       << 8) |
      (static_cast<quint32>(static_cast<quint8>(pdu.at(after_cookie + 6)))
       << 16) |
      (static_cast<quint32>(static_cast<quint8>(pdu.at(after_cookie + 7)))
       << 24);
  if (neg_type != 0x01 || neg_length != 8) {
    if (error_message) {
      *error_message = QStringLiteral("Wrong info on rdpNegReq");
    }
    return false;
  }
  info->requested_rdstls = (requested_protocols & kProtocolRdstls) != 0;
  if (error_message) {
    error_message->clear();
  }
  return true;
}
