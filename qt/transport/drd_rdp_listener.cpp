#include "transport/drd_rdp_listener.h"

#include "core/drd_server_runtime.h"
#include "utils/drd_log.h"
#include <freerdp/listener.h>
#include <freerdp/settings.h>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDebug>

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
 * 逻辑：创建 FreeRDP 监听器，绑定地址和端口，开始监听。
 * 参数：error 错误输出。
 * 外部接口：FreeRDP API 创建和启动监听器。
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
    
    DRD_LOG_MESSAGE("Socket service armed for %s:%u",
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
 * 外部接口：FreeRDP API 停止监听器。
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
    
    // 停止运行时
    if (m_runtime != nullptr)
    {
        m_runtime->stop();
    }
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
    
    // TODO: 实现 FreeRDP peer 创建和配置
    // 1. 从 socket 获取文件描述符
    // 2. 创建 freerdp_peer
    // 3. 配置 peer 设置
    // 4. 启动 peer
    
    // 临时处理：关闭连接
    socket->disconnectFromHost();
    socket->deleteLater();
}