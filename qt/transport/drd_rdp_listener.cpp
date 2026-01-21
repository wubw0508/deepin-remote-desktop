#include "transport/drd_rdp_listener.h"

#include "core/drd_server_runtime.h"
#include "session/drd_rdp_session.h"
#include "security/drd_tls_credentials.h"
#include "utils/drd_log.h"
#include <freerdp/listener.h>
#include <freerdp/settings.h>
#include <freerdp/settings_keys.h>
#include <freerdp/constants.h>
#include <freerdp/freerdp.h>
#include <freerdp/channels/wtsvc.h>
#include <winpr/wtypes.h>
#include <winpr/wtsapi.h>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDebug>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

/**
 * @brief Peer 上下文结构
 *
 * 功能：存储 FreeRDP peer 的自定义上下文数据。
 * 逻辑：包含会话、监听器等引用。
 */
typedef struct
{
    rdpContext context;
    DrdRdpSession *session;
    DrdRdpListener *listener;
} DrdRdpPeerContext;

/**
 * @brief 构造函数
 * 
 * 功能：初始化 RDP 监听器对象。
 * 逻辑：保存监听参数，初始化成员变量。
 * 参数：bindAddress 绑定地址，port 端口，runtime 运行时对象，encodingOptions 编码选项，nlaEnabled 是否启用 NLA，nlaUsername NLA 用户名，nlaPassword NLA 密码，pamService PAM 服务，runtimeMode 运行模式，parent 父对象。
 * 外部接口：Qt QObject 构造函数。
 */
DrdRdpListener::DrdRdpListener(const QString &bindAddress,
                               quint16 port,
                               DrdServerRuntime *runtime,
                               const DrdEncodingOptions *encodingOptions,
                               bool nlaEnabled,
                               const QString &nlaUsername,
                               const QString &nlaPassword,
                               const QString &pamService,
                               DrdRuntimeMode runtimeMode,
                               QObject *parent)
    : QObject(parent)
    , m_bindAddress(bindAddress)
    , m_port(port)
    , m_runtime(runtime)
    , m_nlaEnabled(nlaEnabled)
    , m_nlaUsername(nlaUsername)
    , m_nlaPassword(nlaPassword)
    , m_pamService(pamService)
    , m_runtimeMode(runtimeMode)
    , m_isSingleLogin(false)
{
    if (encodingOptions != nullptr)
    {
        m_encodingOptions = *encodingOptions;
    }
}

/**
 * @brief 析构函数
 * 
 * 功能：清理 RDP 监听器对象。
 * 逻辑：停止监听，清理资源。
 * 参数：无。
 * 外部接口：Qt QObject 析构函数。
 */
DrdRdpListener::~DrdRdpListener()
{
    stop();
}

/**
 * @brief 启动监听器
 * 
 * 功能：启动 RDP 监听器，开始监听连接。
 * 逻辑：创建 TCP 服务器，绑定地址和端口，开始监听。
 * 参数：error 错误输出。
 * 外部接口：Qt QTcpServer API。
 */
bool DrdRdpListener::start(QString *error)
{
    // 创建 TCP 服务器
    QTcpServer *server = new QTcpServer(this);
    
    // 绑定地址和端口
    QHostAddress address;
    if (m_bindAddress.isEmpty() || m_bindAddress == "0.0.0.0")
    {
        address = QHostAddress::Any;
    }
    else if (m_bindAddress == "::")
    {
        address = QHostAddress::AnyIPv6;
    }
    else
    {
        address = QHostAddress(m_bindAddress);
        if (address.isNull())
        {
            if (error)
            {
                *error = QString("Invalid bind address: %1").arg(m_bindAddress);
            }
            delete server;
            return false;
        }
    }
    
    if (!server->listen(address, m_port))
    {
        if (error)
        {
            *error = QString("Failed to bind to %1:%2 - %3")
                    .arg(m_bindAddress.isEmpty() ? "0.0.0.0" : m_bindAddress)
                    .arg(m_port)
                    .arg(server->errorString());
        }
        delete server;
        return false;
    }
    
    // 连接 newConnection 信号
    connect(server, &QTcpServer::newConnection, this, [this, server]() {
        handleIncomingConnection(server);
    });
    
    DRD_LOG_MESSAGE("RDP listener started on %s:%u",
                    m_bindAddress.isEmpty() ? "0.0.0.0" : m_bindAddress.toUtf8().constData(),
                    m_port);
    
    return true;
}

/**
 * @brief 停止监听器
 * 
 * 功能：停止 RDP 监听器，关闭所有连接。
 * 逻辑：停止监听，关闭所有活动连接。
 * 参数：无。
 * 外部接口：Qt QTcpServer API。
 */
void DrdRdpListener::stop()
{
    // 查找并停止所有 QTcpServer 子对象
    QList<QTcpServer*> servers = findChildren<QTcpServer*>();
    for (QTcpServer *server : servers)
    {
        if (server->isListening())
        {
            server->close();
        }
        server->deleteLater();
    }
    
    // 停止所有会话
    for (const QPointer<DrdRdpSession> &session : m_sessions)
    {
        if (session != nullptr)
        {
            session->disconnect("Listener stopping");
        }
    }
    m_sessions.clear();
    
    // 停止运行时
    if (m_runtime != nullptr)
    {
        m_runtime->stop();
    }
}

/**
 * @brief 从 QTcpSocket 获取文件描述符
 *
 * 功能：从 Qt socket 获取底层文件描述符并复制一份。
 * 逻辑：使用 socket descriptor，dup 一份并设置 CLOEXEC。
 * 参数：socket Qt socket。
 * 外部接口：POSIX dup/fcntl。
 */
static int getSocketDescriptor(QTcpSocket *socket)
{
    if (socket == nullptr)
    {
        return -1;
    }
    
    int fd = socket->socketDescriptor();
    if (fd < 0)
    {
        return -1;
    }
    
    // 复制文件描述符
    int duplicated_fd = dup(fd);
    if (duplicated_fd < 0)
    {
        DRD_LOG_WARNING("dup() failed: %s", strerror(errno));
        return -1;
    }
    
    // 设置 CLOEXEC 标志
    int flag = fcntl(duplicated_fd, F_GETFD);
    if (flag >= 0)
    {
        flag |= FD_CLOEXEC;
        fcntl(duplicated_fd, F_SETFD, flag);
    }
    
    return duplicated_fd;
}

/**
 * @brief Peer 上下文创建回调
 *
 * 功能：初始化 peer 上下文。
 * 逻辑：创建并初始化自定义上下文，创建会话对象。
 * 参数：client FreeRDP peer，context FreeRDP 上下文。
 * 外部接口：FreeRDP 回调机制。
 */
static BOOL drd_peer_context_new(freerdp_peer *client, rdpContext *context)
{
    DrdRdpPeerContext *ctx = (DrdRdpPeerContext *)context;
    ctx->session = new DrdRdpSession(client);
    ctx->listener = nullptr;
    return ctx->session != nullptr;
}

/**
 * @brief Peer 上下文释放回调
 *
 * 功能：清理 peer 上下文资源。
 * 逻辑：释放会话引用。
 * 参数：client FreeRDP peer，context FreeRDP 上下文。
 * 外部接口：FreeRDP 回调机制。
 */
static void drd_peer_context_free([[maybe_unused]] freerdp_peer *client, rdpContext *context)
{
    DrdRdpPeerContext *ctx = (DrdRdpPeerContext *)context;
    if (ctx->session != nullptr)
    {
        if (ctx->listener != nullptr)
        {
            // 会话关闭回调会在 session 析构时自动处理
        }
        delete ctx->session;
        ctx->session = nullptr;
    }
    ctx->listener = nullptr;
}

/**
 * @brief Peer PostConnect 回调
 *
 * 功能：处理连接后的初始化。
 * 逻辑：记录日志，更新会话状态。
 * 参数：client FreeRDP peer。
 * 外部接口：FreeRDP 回调机制。
 */
static BOOL drd_peer_post_connect(freerdp_peer *client)
{
    DrdRdpPeerContext *ctx = (DrdRdpPeerContext *)client->context;
    if (ctx == nullptr || ctx->session == nullptr)
    {
        return FALSE;
    }
    ctx->session->setPeerState("post-connect");
    return TRUE;
}

/**
 * @brief Peer Activate 回调
 *
 * 功能：激活 peer 会话。
 * 逻辑：记录日志，更新会话状态。
 * 参数：client FreeRDP peer。
 * 外部接口：FreeRDP 回调机制。
 */
static BOOL drd_peer_activate(freerdp_peer *client)
{
    DrdRdpPeerContext *ctx = (DrdRdpPeerContext *)client->context;
    if (ctx == nullptr || ctx->session == nullptr)
    {
        return FALSE;
    }
    ctx->session->setPeerState("activated");
    return TRUE;
}

/**
 * @brief Peer 断开连接回调
 *
 * 功能：处理 peer 断开事件。
 * 逻辑：记录日志，更新会话状态，停止事件线程。
 * 参数：client FreeRDP peer。
 * 外部接口：FreeRDP 回调机制。
 */
static void drd_peer_disconnected(freerdp_peer *client)
{
    DrdRdpPeerContext *ctx = (DrdRdpPeerContext *)client->context;
    if (ctx == nullptr || ctx->session == nullptr)
    {
        return;
    }
    ctx->session->setPeerState("disconnected");
    ctx->session->stopEventThread();
}

/**
 * @brief 会话关闭回调函数
 *
 * 功能：处理会话关闭事件。
 * 逻辑：从监听器的会话列表中移除会话。
 * 参数：session 关闭的会话，user_data 监听器对象。
 * 外部接口：无。
 */
static void drd_rdp_listener_session_closed(DrdRdpSession *session, void *user_data)
{
    DrdRdpListener *listener = static_cast<DrdRdpListener *>(user_data);
    if (listener == nullptr)
    {
        return;
    }

    // 使用 QMetaObject::invokeMethod 确保在主线程中执行
    QMetaObject::invokeMethod(listener, [listener, session]() {
        listener->handleSessionClosed(session);
    }, Qt::QueuedConnection);
}

/**
 * @brief 配置 peer 设置
 *
 * 功能：配置 FreeRDP peer 的各种设置。
 * 逻辑：应用 TLS 证书，设置桌面尺寸、编码选项等。
 * 参数：listener 监听器，client FreeRDP peer，peerName 对端名称。
 * 外部接口：FreeRDP settings API。
 */
static bool configurePeerSettings(DrdRdpListener *listener, freerdp_peer *client, const QString &peerName)
{
    if (client->context == nullptr)
    {
        DRD_LOG_WARNING("Peer %s context is null", peerName.toUtf8().constData());
        return false;
    }
    
    rdpSettings *settings = client->context->settings;
    if (settings == nullptr)
    {
        DRD_LOG_WARNING("Peer %s settings is null", peerName.toUtf8().constData());
        return false;
    }
    
    // 获取 TLS 凭据
    DrdTlsCredentials *tls = listener->runtime()->tlsCredentials();
    if (tls == nullptr)
    {
        DRD_LOG_WARNING("TLS credentials not configured");
        return false;
    }
    
    QString error;
    if (!tls->apply(settings, &error))
    {
        DRD_LOG_WARNING("Failed to apply TLS credentials: %s", error.toUtf8().constData());
        return false;
    }
    
    // 设置桌面尺寸
    const UINT32 width = listener->encodingOptions().width;
    const UINT32 height = listener->encodingOptions().height;
    
    if (width == 0 || height == 0)
    {
        DRD_LOG_WARNING("Encoding geometry is not configured");
        return false;
    }
    
    // 配置基本设置
    if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, width) ||
        !freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, height) ||
        !freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32) ||
        !freerdp_settings_set_bool(settings, FreeRDP_ServerMode, TRUE) ||
        !freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel, ENCRYPTION_LEVEL_CLIENT_COMPATIBLE))
    {
        DRD_LOG_WARNING("Failed to configure peer settings");
        return false;
    }
    
    // 配置安全设置
    if (!listener->nlaEnabled())
    {
        // TLS-only 模式
        if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE) ||
            !freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE) ||
            !freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, FALSE))
        {
            DRD_LOG_WARNING("Failed to configure TLS-only security flags");
            return false;
        }
    }
    else
    {
        // NLA 模式
        if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE) ||
            !freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, TRUE) ||
            !freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, FALSE))
        {
            DRD_LOG_WARNING("Failed to configure NLA security flags");
            return false;
        }
    }
    
    // Handover 模式启用 RDSTLS
    if (listener->isHandoverMode())
    {
        if (!freerdp_settings_set_bool(settings, FreeRDP_RdstlsSecurity, TRUE))
        {
            DRD_LOG_WARNING("Failed to enable RDSTLS security flags");
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 处理传入的连接
 *
 * 功能：处理新的 RDP 连接。
 * 逻辑：从服务器获取连接，创建 FreeRDP peer，配置并启动会话。
 * 参数：server TCP 服务器。
 * 外部接口：FreeRDP API 创建和配置 peer。
 */
void DrdRdpListener::handleIncomingConnection(QTcpServer *server)
{
    QTcpSocket *socket = server->nextPendingConnection();
    if (socket == nullptr)
    {
        return;
    }
    
    // 获取对端地址
    QString peerAddress = socket->peerAddress().toString();
    quint16 peerPort = socket->peerPort();
    QString peerName = QString("%1:%2").arg(peerAddress).arg(peerPort);
    
    DRD_LOG_MESSAGE("Incoming connection from %s", peerName.toUtf8().constData());
    
    // 1. 从 socket 获取文件描述符
    int fd = getSocketDescriptor(socket);
    if (fd < 0)
    {
        DRD_LOG_WARNING("Failed to get socket descriptor for %s", peerName.toUtf8().constData());
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    
    // 2. 创建 freerdp_peer
    freerdp_peer *peer = freerdp_peer_new(fd);
    if (peer == nullptr)
    {
        DRD_LOG_WARNING("Failed to create FreeRDP peer for %s", peerName.toUtf8().constData());
        close(fd);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    
    // 设置 peer 上下文
    peer->ContextSize = sizeof(DrdRdpPeerContext);
    peer->ContextNew = drd_peer_context_new;
    peer->ContextFree = drd_peer_context_free;
    
    // 初始化 peer 上下文（这会调用 drd_peer_context_new 回调）
    if (!freerdp_peer_context_new(peer))
    {
        DRD_LOG_WARNING("Failed to allocate peer context for %s", peerName.toUtf8().constData());
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    
    // 检查是否有活动会话
    if (hasActiveSession())
    {
        DRD_LOG_WARNING("Rejecting connection from %s: session already active", peerName.toUtf8().constData());
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    
    // 配置 peer 设置
    if (!configurePeerSettings(this, peer, peerName))
    {
        DRD_LOG_WARNING("Failed to configure peer settings for %s", peerName.toUtf8().constData());
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    
    // 设置 peer 回调
    peer->PostConnect = drd_peer_post_connect;
    peer->Activate = drd_peer_activate;
    peer->Disconnect = drd_peer_disconnected;
    
    // 初始化 peer
    if (peer->Initialize == nullptr || !peer->Initialize(peer))
    {
        DRD_LOG_WARNING("Failed to initialize peer for %s", peerName.toUtf8().constData());
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    
    // 获取 peer 上下文
    DrdRdpPeerContext *ctx = (DrdRdpPeerContext *)peer->context;
    if (ctx == nullptr || ctx->session == nullptr)
    {
        DRD_LOG_WARNING("Peer %s context missing session", peerName.toUtf8().constData());
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    
    // 设置会话属性
    ctx->session->setPeerAddress(peerName);
    ctx->listener = this;
    ctx->session->setClosedCallback(drd_rdp_listener_session_closed, this);
    
    // 启动事件线程
    if (!ctx->session->startEventThread())
    {
        DRD_LOG_WARNING("Failed to start event thread for %s", peerName.toUtf8().constData());
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    
    // 添加到会话列表
    m_sessions.append(QPointer<DrdRdpSession>(ctx->session));
    
    DRD_LOG_MESSAGE("Accepted connection from %s", peerName.toUtf8().constData());
    
    // 关闭 Qt socket（FreeRDP 现在拥有文件描述符）
    socket->disconnectFromHost();
    socket->deleteLater();
}

/**
 * @brief 处理会话关闭
 *
 * 功能：从会话列表中移除已关闭的会话。
 * 逻辑：查找并移除会话，记录日志。
 * 参数：session 关闭的会话。
 * 外部接口：无。
 */
void DrdRdpListener::handleSessionClosed(DrdRdpSession *session)
{
    if (session == nullptr)
    {
        return;
    }

    // 从会话列表中移除
    for (int i = 0; i < m_sessions.size(); ++i)
    {
        if (m_sessions[i] == session)
        {
            m_sessions.removeAt(i);
            DRD_LOG_MESSAGE("Session %s closed, %d session(s) remaining",
                          session->peerAddress().toUtf8().constData(),
                          m_sessions.size());
            break;
        }
    }
}