#include "transport/drd_rdp_listener.h"

#include "core/drd_server_runtime.h"
#include "session/drd_rdp_session.h"
#include "security/drd_tls_credentials.h"
#include <freerdp/listener.h>
#include <freerdp/settings.h>
#include <freerdp/settings_keys.h>
#include <freerdp/constants.h>
#include <freerdp/freerdp.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/drdynvc.h>  // 添加 DRDYNVC 通道支持
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
    HANDLE vcm;  // Virtual Channel Manager
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
    , m_sessionCallback(nullptr)
    , m_sessionCallbackData(nullptr)
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
    
    qInfo() << "RDP listener started on"
            << (m_bindAddress.isEmpty() ? "0.0.0.0" : m_bindAddress) << ":" << m_port;
    
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
        qWarning() << "dup() failed:" << strerror(errno);
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
    ctx->listener = NULL;
    ctx->vcm = INVALID_HANDLE_VALUE;  // 初始化为无效值
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
    
    // 清理 Virtual Channel Manager
    if (ctx->vcm != nullptr && ctx->vcm != INVALID_HANDLE_VALUE)
    {
        WTSCloseServer(ctx->vcm);
        ctx->vcm = INVALID_HANDLE_VALUE;
    }
    
    ctx->listener = nullptr;
}

/**
 * @brief 检查监听器是否运行在 system 模式
 *
 * 功能：判断监听器是否运行在 system 模式。
 * 逻辑：检查实例非空且 runtime_mode 为 DRD_RUNTIME_MODE_SYSTEM。
 * 参数：listener 监听器。
 * 外部接口：无。
 */
static bool drd_rdp_listener_is_system_mode(DrdRdpListener *listener)
{
    return listener != nullptr && listener->runtimeMode() == DrdRuntimeMode::System;
}

/**
 * @brief 在 system 模式下进行 TLS 登录认证
 *
 * 功能：在非 NLA 模式下执行 TLS 认证。
 * 逻辑：读取客户端凭据，进行 TLS 认证。
 * 参数：ctx peer 上下文，client FreeRDP peer。
 * 外部接口：FreeRDP settings API。
 */
static bool drd_rdp_listener_authenticate_tls_login(DrdRdpPeerContext *ctx, freerdp_peer *client)
{
    if (ctx == nullptr || ctx->session == nullptr || ctx->listener == nullptr || client == nullptr ||
        client->context == nullptr || client->context->settings == nullptr)
    {
        qWarning() << "[TLS-AUTH] Missing context or settings for TLS authentication";
        return false;
    }

    rdpSettings *settings = client->context->settings;
    const char *username = freerdp_settings_get_string(settings, FreeRDP_Username);
    const char *password = freerdp_settings_get_string(settings, FreeRDP_Password);
    const char *domain = freerdp_settings_get_string(settings, FreeRDP_Domain);
    
    if (username == nullptr || *username == '\0' || password == nullptr || *password == '\0')
    {
        qWarning() << "[TLS-AUTH] Peer" << client->hostname << "missing credentials in TLS client info";
        return false;
    }

    qInfo() << "[TLS-AUTH] Peer" << client->hostname << "TLS authentication for user" << username;
    
    // TODO: 实现 TLS/PAM 认证逻辑（目前先返回成功）
    // 注意：PAM 认证暂时跳过，只记录日志
    qWarning() << "[TLS-AUTH] PAM authentication not implemented yet, skipping for user" << username;
    
    // 清空密码（安全考虑）
    freerdp_settings_set_string(settings, FreeRDP_Password, "");
    
    return true;
}

/**
 * @brief Peer PostConnect 回调
 *
 * 功能：处理连接后的初始化。
 * 逻辑：调用会话 post_connect，清理 NLA 资源；在非 NLA 模式下执行 TLS 认证。
 * 参数：client FreeRDP peer。
 * 外部接口：FreeRDP 回调机制。
 */
static BOOL
drd_peer_post_connect(freerdp_peer *client)
{
    qInfo() << "[POSTCONNECT] drd_peer_post_connect called for peer" << client->hostname;
    DrdRdpPeerContext *ctx = (DrdRdpPeerContext *)client->context;
    if (ctx == nullptr || ctx->session == nullptr)
    {
        qWarning() << "[POSTCONNECT] drd_peer_post_connect: context or session is NULL";
        return FALSE;
    }
    
    // 调用会话的 post_connect 处理
    BOOL result = ctx->session->postConnect();
    
    // 清理 NLA SAM 文件（参考 C 版本实现）
    // 注意：Qt 版本可能没有 NLA SAM 文件，但保持与 C 版本一致的清理逻辑
    // g_clear_pointer(&ctx->nla_sam, drd_nla_sam_file_free);
    
    if (!result)
    {
        qWarning() << "[POSTCONNECT] Session post-connect failed for peer" << client->hostname;
        return FALSE;
    }

    // 在非 NLA 模式下执行 TLS 认证（system 模式）
    if (ctx->listener != nullptr && !ctx->listener->nlaEnabled())
    {
        if (drd_rdp_listener_is_system_mode(ctx->listener))
        {
            if (!drd_rdp_listener_authenticate_tls_login(ctx, client))
            {
                qWarning() << "[POSTCONNECT] TLS authentication failed for peer" << client->hostname;
                ctx->session->disconnect("tls-rdp-sso-auth-failed");
                return FALSE;
            }
        }
    }

    // 调用会话回调（如果设置了）
    if (ctx->listener != nullptr)
    {
        ctx->listener->invokeSessionCallback(ctx->session);
    }
    
    qInfo() << "[POSTCONNECT] drd_peer_post_connect completed successfully for peer" << client->hostname;
    return TRUE;
}

/**
 * @brief Peer Activate 回调
 *
 * 功能：激活 peer 会话。
 * 逻辑：记录日志，更新会话状态，启动抓取和编码流程。
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
    return ctx->session->activate();
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
 * @brief Peer Capabilities 回调
 *
 * 功能：处理客户端能力协商。
 * 逻辑：检查客户端是否支持DRDYNVC和DesktopResize能力。
 * 参数：client FreeRDP peer。
 * 外部接口：FreeRDP 回调机制。
 */
static BOOL drd_peer_capabilities(freerdp_peer *client)
{
    if (client == nullptr || client->context == nullptr)
    {
        return FALSE;
    }
    
    rdpContext *context = client->context;
    rdpSettings *settings = context->settings;
    if (settings == nullptr)
    {
        return FALSE;
    }
    
    const quint32 client_width = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
    const quint32 client_height = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
    const bool desktop_resize = freerdp_settings_get_bool(settings, FreeRDP_DesktopResize);
    
    DrdRdpPeerContext *ctx = (DrdRdpPeerContext *)client->context;
    
    // 检查 Virtual Channel Manager 是否存在
    if (ctx == nullptr || ctx->vcm == nullptr || ctx->vcm == INVALID_HANDLE_VALUE)
    {
        qWarning() << "Peer" << client->hostname << "missing virtual channel manager during capability exchange";
        return FALSE;
    }
    
    // 检查 DRDYNVC 通道是否已加入
    if (!WTSVirtualChannelManagerIsChannelJoined(ctx->vcm, DRDYNVC_SVC_CHANNEL_NAME))
    {
        qWarning() << "Peer" << client->hostname << "does not support DRDYNVC, rejecting connection";
        return FALSE;
    }
    
    if (!desktop_resize)
    {
        if (ctx != nullptr && ctx->session != nullptr)
        {
            ctx->session->setPeerState("desktop-resize-unsupported");
        }
        qWarning() << "Peer" << client->hostname << "disabled DesktopResize capability (client" << client_width << "x" << client_height << "), rejecting connection";
        return FALSE;
    }
    
    // 在 system 模式下更新编码配置
    if (ctx != nullptr && ctx->listener != nullptr)
    {
        // 调用 system 模式编码更新（需要实现对应的函数）
        // drd_rdp_listener_update_system_encoding(ctx->listener, client_width, client_height);
        qDebug() << "System mode encoding update would be called for client resolution" << client_width << "x" << client_height;
    }
    
    qInfo() << "Peer" << client->hostname << "capabilities accepted with DesktopResize enabled (" << client_width << "x" << client_height << "requested)";
    return TRUE;
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
        qWarning() << "Peer" << peerName << "context is null";
        return false;
    }
    
    rdpSettings *settings = client->context->settings;
    if (settings == nullptr)
    {
        qWarning() << "Peer" << peerName << "settings is null";
        return false;
    }
    
    // 获取 TLS 凭据
    DrdTlsCredentials *tls = listener->runtime()->tlsCredentials();
    if (tls == nullptr)
    {
        qWarning() << "TLS credentials not configured";
        return false;
    }
    
    QString error;
    if (!tls->apply(settings, &error))
    {
        qWarning() << "Failed to apply TLS credentials:" << error;
        return false;
    }
    
    // 设置桌面尺寸
    const UINT32 width = listener->encodingOptions().width;
    const UINT32 height = listener->encodingOptions().height;
    
    if (width == 0 || height == 0)
    {
        qWarning() << "Encoding geometry is not configured";
        return false;
    }
    
    // 配置基本设置
    if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, width) ||
        !freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, height) ||
        !freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32) ||
        !freerdp_settings_set_bool(settings, FreeRDP_ServerMode, TRUE) ||
        !freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel, ENCRYPTION_LEVEL_CLIENT_COMPATIBLE))
    {
        qWarning() << "Failed to configure peer settings";
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
            qWarning() << "Failed to configure TLS-only security flags";
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
            qWarning() << "Failed to configure NLA security flags";
            return false;
        }
    }
    
    // Handover 模式启用 RDSTLS
    if (listener->isHandoverMode())
    {
        if (!freerdp_settings_set_bool(settings, FreeRDP_RdstlsSecurity, TRUE))
        {
            qWarning() << "Failed to enable RDSTLS security flags";
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 从 QTcpSocket 创建 FreeRDP peer
 *
 * 功能：从 Qt socket 创建 FreeRDP peer 对象。
 * 逻辑：调用 getSocketDescriptor 获取文件描述符，创建 FreeRDP peer。
 * 参数：socket Qt TCP socket，error 错误信息输出。
 * 外部接口：FreeRDP freerdp_peer_new API。
 */
freerdp_peer *DrdRdpListener::drd_rdp_listener_peer_from_connection(QTcpSocket *socket, QString *error)
{
    if (socket == nullptr)
    {
        if (error)
        {
            *error = "Socket is null";
        }
        return nullptr;
    }
    
    // 获取对端地址用于日志
    QString peerAddress = socket->peerAddress().toString();
    quint16 peerPort = socket->peerPort();
    QString peerName = QString("%1:%2").arg(peerAddress).arg(peerPort);
    
    qInfo() << "Creating FreeRDP peer from connection" << peerName;
    
    // 使用现有的 getSocketDescriptor 函数获取文件描述符
    int duplicated_fd = getSocketDescriptor(socket);
    if (duplicated_fd < 0)
    {
        QString errMsg = QString("Failed to get socket descriptor for %1").arg(peerName);
        qWarning() << errMsg;
        if (error)
        {
            *error = errMsg;
        }
        return nullptr;
    }
    
    // 创建 freerdp_peer
    freerdp_peer *peer = freerdp_peer_new(duplicated_fd);
    if (peer == nullptr)
    {
        QString errMsg = QString("freerdp_peer_new returned null for %1").arg(peerName);
        qWarning() << errMsg;
        close(duplicated_fd);
        if (error)
        {
            *error = errMsg;
        }
        return nullptr;
    }
    
    qDebug() << "FreeRDP peer created successfully for" << peerName;
    return peer;
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

    qInfo() << "Incoming connection from" << peerName;

    // 使用封装的函数创建 FreeRDP peer
    QString peerError;
    freerdp_peer *peer = drd_rdp_listener_peer_from_connection(socket, &peerError);
    if (peer == nullptr)
    {
        qWarning() << "Failed to create FreeRDP peer:" << peerError;
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    qDebug() << "FreeRDP peer created successfully for" << peerName;

    // 设置 peer 上下文
    peer->ContextSize = sizeof(DrdRdpPeerContext);
    peer->ContextNew = drd_peer_context_new;
    peer->ContextFree = drd_peer_context_free;

    // 初始化 peer 上下文（这会调用 drd_peer_context_new 回调）
    if (!freerdp_peer_context_new(peer))
    {
        qWarning() << "Failed to allocate peer context for" << peerName;
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    qDebug() << "FreeRDP peer context allocated for" << peerName;

    // 检查是否有活动会话（在配置设置之前检查）
    if (hasActiveSession())
    {
        qWarning() << "Rejecting connection from" << peerName << ": session already active";
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    qDebug() << "No active session found, proceeding with connection for" << peerName;

    // 配置 peer 设置
    if (!configurePeerSettings(this, peer, peerName))
    {
        qWarning() << "Failed to configure peer settings for" << peerName;
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    qDebug() << "Peer settings configured successfully for" << peerName;

    // 设置 peer 回调
    peer->PostConnect = drd_peer_post_connect;
    peer->Activate = drd_peer_activate;
    peer->Disconnect = drd_peer_disconnected;
    peer->Capabilities = drd_peer_capabilities;

    qDebug() << "FreeRDP callbacks set for" << peerName;

    // 初始化 peer
    qInfo() << "Initializing peer for" << peerName;
    if (peer->Initialize == nullptr)
    {
        qWarning() << "Peer Initialize callback is NULL for" << peerName;
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    qDebug() << "Peer Initialize callback is available, calling Initialize() for" << peerName;

    if (!peer->Initialize(peer))
    {
        qWarning() << "Failed to initialize peer for" << peerName;
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    qInfo() << "Peer initialized successfully for" << peerName;
    qDebug() << "Peer connected state after initialization:" << peer->connected;

    // 获取 peer 上下文
    DrdRdpPeerContext *ctx = (DrdRdpPeerContext *)peer->context;
    if (ctx == nullptr || ctx->session == nullptr)
    {
        qWarning() << "Peer" << peerName << "context missing session";
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    qDebug() << "Peer context and session found for" << peerName;

    // 设置会话属性（参考C版本第1308-1321行）
    ctx->session->setPeerAddress(peerName);

    // 创建 Virtual Channel Manager（使用 WTSOpenServerA，参考C版本第1310行）
    ctx->vcm = WTSOpenServerA((LPSTR)peer->context);
    if (ctx->vcm == nullptr || ctx->vcm == INVALID_HANDLE_VALUE)
    {
        qWarning() << "Failed to create virtual channel manager for" << peerName;
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    qDebug() << "Virtual Channel Manager created successfully for" << peerName;
    
    // 设置 Virtual Channel Manager 到会话（参考C版本第1317行）
    ctx->session->setVirtualChannelManager(ctx->vcm);

    // 设置 runtime 和被动模式（参考C版本第1319-1321行）
    ctx->listener = this;
    ctx->session->setRuntime(m_runtime);
    ctx->session->setPassiveMode(drd_rdp_listener_is_system_mode(this));
    
    // 设置关闭回调（参考C版本第1332-1334行）
    ctx->session->setClosedCallback(drd_rdp_listener_session_closed, this);
    
    // 设置peer状态（参考C版本第1330行）
    ctx->session->setPeerState("initialized");
    
    qDebug() << "Session properties set for" << peerName;
    qInfo() << "Virtual Channel Manager created and set for session" << peerName;
    
    // 添加到会话列表（参考C版本第1331行）
    m_sessions.append(QPointer<DrdRdpSession>(ctx->session));
    
    // 关键修复：参考C版本实现，直接启动线程，不检查连接状态
    // 在连接建立阶段，peer->connected可能为0，但连接仍在建立中
    // FreeRDP会在握手过程中更新连接状态
    qInfo() << "Starting event and VCM threads (connection establishment in progress)";
    qDebug() << "Peer state - connected:" << (peer ? peer->connected : false);
    
    // 启动事件线程 - 这是连接成功的关键（参考C版本第1323行）
    qInfo() << "Starting event thread for" << peerName;
    if (!ctx->session->startEventThread())
    {
        qWarning() << "Failed to start event thread for" << peerName;
        freerdp_peer_free(peer);
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    qInfo() << "Event thread started for" << peerName;
    qDebug() << "Event thread status: connectionAlive=" << ctx->session->isConnectionAlive();
    
    // 启动 VCM 线程（监控 drdynvc 状态）
    qInfo() << "Starting VCM thread for" << peerName;
    if (!ctx->session->startVcmThread())
    {
        qWarning() << "Failed to start VCM thread for" << peerName;
        // VCM 线程失败不是致命错误，继续
    }
    else
    {
        qInfo() << "VCM thread started for" << peerName;
        qDebug() << "VCM thread started successfully";
    }
    m_sessions.append(QPointer<DrdRdpSession>(ctx->session));
    
    qInfo() << "Accepted connection from" << peerName;
    
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
            qInfo() << "handleSessionClosed Session" << session->peerAddress() << "closed," << m_sessions.size() << "session(s) remaining";
            
            // 关键修复：确保会话相关的资源被正确清理
            // session 对象会在 Qt 对象树中自动清理其子对象（包括 socket）
            break;
        }
    }
}

/**
    * @brief 设置会话回调函数
    *
    * 功能：设置会话建立后的回调函数。
    * 逻辑：保存回调函数和用户数据，在会话建立后调用。
    * 参数：func 回调函数，user_data 用户数据。
    */
void DrdRdpListener::setSessionCallback(DrdRdpListener::SessionCallbackFunc func, void *user_data)
{
    m_sessionCallback = func;
    m_sessionCallbackData = user_data;
}

/**
 * @brief 调用会话回调
 *
 * 功能：调用已设置的会话回调函数。
 * 逻辑：检查回调函数是否设置，如果设置则调用。
 * 参数：session 会话对象。
 */
void DrdRdpListener::invokeSessionCallback(DrdRdpSession *session)
{
    if (m_sessionCallback != nullptr)
    {
        m_sessionCallback(this, session, m_sessionCallbackData);
    }
}
