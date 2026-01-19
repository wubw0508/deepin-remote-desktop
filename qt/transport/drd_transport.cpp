#include "drd_transport.h"
#include "drd_rdp_listener.moc"

#include "core/drd_server_runtime.h"
#include "security/drd_tls_credentials.h"

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>

#include "session/drd_rdp_session.h"

#include <freerdp/freerdp.h>

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

// 配置 FreeRDP 对等体设置
bool configure_peer_settings(freerdp_peer *peer, DrdQtRdpListener *listener, QString *error_message) {
  if (!peer || !peer->context) {
    if (error_message) {
      *error_message = QStringLiteral("Invalid peer context");
    }
    return false;
  }

  rdpSettings *settings = peer->context->settings;
  if (!settings) {
    if (error_message) {
      *error_message = QStringLiteral("Failed to get peer settings");
    }
    return false;
  }

  // 获取 TLS 证书
  DrdQtServerRuntime *runtime = qobject_cast<DrdQtServerRuntime*>(listener->runtime());
  if (!runtime) {
    if (error_message) {
      *error_message = QStringLiteral("Invalid server runtime");
    }
    return false;
  }

  DrdQtTlsCredentials *tls = runtime->tlsCredentials();
  if (!tls) {
    if (error_message) {
      *error_message = QStringLiteral("TLS credentials not configured");
    }
    return false;
  }

  if (!tls->apply(settings, error_message)) {
    return false;
  }

  // 配置安全设置
  if (listener->nla_enabled()) {
    // NLA 安全模式
    if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, false) ||
        !freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, true) ||
        !freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, false)) {
      if (error_message) {
        *error_message = QStringLiteral("Failed to configure NLA security flags");
      }
      return false;
    }
  } else {
    // TLS 安全模式
    if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, true) ||
        !freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, false) ||
        !freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, false)) {
      if (error_message) {
        *error_message = QStringLiteral("Failed to configure TLS security flags");
      }
      return false;
    }
    qInfo() << "Peer will authenticate via TLS/PAM service" << listener->pam_service();
  }

  // 配置桌面尺寸和颜色深度
  const int width = listener->encoding_options().value("width", 1024).toInt();
  const int height = listener->encoding_options().value("height", 768).toInt();
  if (width == 0 || height == 0) {
    if (error_message) {
      *error_message = QStringLiteral("Encoding geometry is not configured");
    }
    return false;
  }

  if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, width) ||
      !freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, height) ||
      !freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32) ||
      !freerdp_settings_set_bool(settings, FreeRDP_ServerMode, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_SurfaceFrameMarkerEnabled, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_FrameMarkerCommandEnabled, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_FastPathOutput, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_RefreshRect, false) ||
      !freerdp_settings_set_bool(settings, FreeRDP_SupportDisplayControl, false) ||
      !freerdp_settings_set_bool(settings, FreeRDP_SupportMonitorLayoutPdu, false) ||
      !freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_RemoteFxImageCodec, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_NSCodec, false) ||
      !freerdp_settings_set_bool(settings, FreeRDP_GfxH264, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, false) ||
      !freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, false) ||
      !freerdp_settings_set_bool(settings, FreeRDP_GfxProgressive, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_GfxProgressiveV2, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_HasExtendedMouseEvent, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_HasHorizontalWheel, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_HasRelativeMouseEvent, false) ||
      !freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_HasQoeEvent, false) ||
      !freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel, ENCRYPTION_LEVEL_CLIENT_COMPATIBLE) ||
      !freerdp_settings_set_uint32(settings, FreeRDP_VCFlags, VCCAPS_COMPR_SC) ||
      !freerdp_settings_set_uint32(settings, FreeRDP_VCChunkSize, 16256) ||
      !freerdp_settings_set_uint32(settings, FreeRDP_PointerCacheSize, 100) ||
      !freerdp_settings_set_uint32(settings, FreeRDP_MultifragMaxRequestSize, 0) ||
      !freerdp_settings_set_uint32(settings, FreeRDP_OsMajorType, OSMAJORTYPE_UNIX) ||
      !freerdp_settings_set_uint32(settings, FreeRDP_OsMinorType, OSMINORTYPE_PSEUDO_XSERVER) ||
      !freerdp_settings_set_bool(settings, FreeRDP_GfxSmallCache, false) ||
      !freerdp_settings_set_bool(settings, FreeRDP_GfxThinClient, true) ||
      !freerdp_settings_set_bool(settings, FreeRDP_SupportMultitransport, false)) {
    if (error_message) {
      *error_message = QStringLiteral("Failed to configure peer settings");
    }
    return false;
  }

  // 配置 RDSTLS 模式
  if (listener->is_handover_mode()) {
    if (!freerdp_settings_set_bool(settings, FreeRDP_RdstlsSecurity, true)) {
      if (error_message) {
        *error_message = QStringLiteral("Failed to enable RDSTLS security flags");
      }
      return false;
    }
  }

  return true;
}

// 初始化 FreeRDP 对等体
bool initialize_peer(freerdp_peer *peer, DrdQtRdpSession *session, const QString &peer_name, QString *error_message) {
  // 初始化对等体
  if (!peer->Initialize || !peer->Initialize(peer)) {
    if (error_message) {
      *error_message = QStringLiteral("Failed to initialize peer");
    }
    return false;
  }

  // 打开虚拟通道管理器
  HANDLE vcm = WTSOpenServerA((LPSTR)peer->context);
  if (!vcm || vcm == INVALID_HANDLE_VALUE) {
    if (error_message) {
      *error_message = QStringLiteral("Failed to create virtual channel manager");
    }
    return false;
  }

  session->setVirtualChannelManager(vcm);

  // 启动事件线程
  if (!session->startEventThread()) {
    if (error_message) {
      *error_message = QStringLiteral("Failed to start event thread");
    }
    WTSCloseServer(vcm);
    return false;
  }

  // 检查输入接口
  if (peer->context && peer->context->input) {
    rdpInput *input = peer->context->input;
    input->context = peer->context;
    // 可以在这里设置输入事件处理函数
  }

  qInfo() << "Accepted connection from" << peer_name;
  return true;
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
  qInfo() << "Starting RDP listener on" << bind_address_ << ":" << port_;
  
  if (running_) {
    qInfo() << "RDP listener already running on" << bind_address_ << ":" << port_;
    if (error_message) {
      error_message->clear();
    }
    return true;
  }
  
  if (!runtime_) {
    qCritical() << "Listener runtime is required";
    if (error_message) {
      *error_message = QStringLiteral("Listener runtime is required");
    }
    return false;
  }
  
  if (pam_service_.isEmpty()) {
    qCritical() << "PAM service is required";
    if (error_message) {
      *error_message = QStringLiteral("PAM service is required");
    }
    return false;
  }
  
  if (nla_enabled_ && (nla_username_.isEmpty() || nla_password_.isEmpty())) {
    qCritical() << "NLA credentials are required";
    if (error_message) {
      *error_message = QStringLiteral("NLA credentials are required");
    }
    return false;
  }
  
  if (!runtime_mode_matches(runtime_mode_name_, "user") &&
      !runtime_mode_matches(runtime_mode_name_, "system") &&
      !runtime_mode_matches(runtime_mode_name_, "handover")) {
    qCritical() << "Invalid runtime mode:" << runtime_mode_name_;
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
        qInfo() << "New incoming connection from" << socket->peerAddress().toString() << ":" << socket->peerPort();
        QString accept_error;
        if (!adopt_connection(socket, &accept_error)) {
          qWarning() << "Failed to adopt connection from" << socket->peerAddress().toString() << ":" << socket->peerPort() << "-" << accept_error;
          socket->close();
          socket->deleteLater();
        } else {
          qInfo() << "Successfully adopted connection from" << socket->peerAddress().toString() << ":" << socket->peerPort();
        }
      }
    });
  }
  
  QHostAddress address;
  if (!address.setAddress(bind_address_)) {
    qCritical() << "Invalid bind address:" << bind_address_;
    if (error_message) {
      *error_message = QStringLiteral("Invalid bind address");
    }
    return false;
  }
  
  if (!server_->listen(address, port_)) {
    qCritical() << "Failed to listen on" << bind_address_ << ":" << port_ << "-" << server_->errorString();
    if (error_message) {
      *error_message = server_->errorString();
    }
    return false;
  }
  
  running_ = true;
  qInfo() << "RDP listener successfully started on" << bind_address_ << ":" << port_;
  
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
  
  // 从 QTcpSocket 中获取文件描述符
  QTcpSocket *socket = qobject_cast<QTcpSocket*>(connection);
  if (!socket) {
    if (error_message) {
      *error_message = QStringLiteral("Connection is not a TCP socket");
    }
    return false;
  }
  
  int sockfd = socket->socketDescriptor();
  if (sockfd == -1) {
    if (error_message) {
      *error_message = QStringLiteral("Failed to get socket descriptor");
    }
    return false;
  }
  
  // 创建 FreeRDP 对等体
  freerdp_peer *peer = freerdp_peer_new(sockfd);
  if (!peer) {
    if (error_message) {
      *error_message = QStringLiteral("Failed to create FreeRDP peer");
    }
    return false;
  }

  // 设置本地地址和主机名
  struct sockaddr_storage peer_addr;
  socklen_t peer_addr_len = sizeof(peer_addr);
  if (getpeername(sockfd, (struct sockaddr*)&peer_addr, &peer_addr_len) == -1) {
    if (error_message) {
      *error_message = QStringLiteral("Failed to get peer address");
    }
    freerdp_peer_free(peer);
    return false;
  }

  if (!freerdp_peer_set_local_and_hostname(peer, &peer_addr)) {
    if (error_message) {
      *error_message = QStringLiteral("Failed to set peer local and hostname");
    }
    freerdp_peer_free(peer);
    return false;
  }

  // 关键修复：必须先设置上下文大小，然后调用 freerdp_peer_context_new 来初始化 peer->context
  // 这是与 C 版本 src/transport/drd_rdp_listener.c 保持一致的做法
  peer->ContextSize = sizeof(rdpContext); // 设置上下文大小
  if (!freerdp_peer_context_new(peer)) { // 初始化上下文
    if (error_message) {
      *error_message = QStringLiteral("Failed to allocate peer context");
    }
    freerdp_peer_free(peer);
    return false;
  }

  // 配置 FreeRDP 对等体设置
  if (!configure_peer_settings(peer, this, error_message)) {
    freerdp_peer_free(peer);
    return false;
  }

  // 创建 RDP 会话
  auto *session = new DrdQtRdpSession(peer, this);

  // 设置会话属性
  session->setPeerAddress(socket->peerAddress().toString());
  session->setRuntime(qobject_cast<DrdQtServerRuntime*>(runtime_));

  // 初始化 FreeRDP 对等体
  if (!initialize_peer(peer, session, socket->peerAddress().toString(), error_message)) {
    delete session;
    freerdp_peer_free(peer);
    return false;
  }

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
