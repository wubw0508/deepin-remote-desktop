#pragma once

#include <QObject>
#include <QString>

#include "core/drd_encoding_options.h"

// 前向声明
class DrdServerRuntime;
class QTcpServer;

/**
 * @brief Qt 版本的 DrdRdpListener 类
 * 
 * 替代 GObject 版本的 DrdRdpListener，使用 Qt 的对象系统
 */
class DrdRdpListener : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param bindAddress 绑定地址
     * @param port 端口
     * @param runtime 运行时对象
     * @param encodingOptions 编码选项
     * @param nlaEnabled 是否启用 NLA
     * @param nlaUsername NLA 用户名
     * @param nlaPassword NLA 密码
     * @param pamService PAM 服务
     * @param runtimeMode 运行模式
     * @param parent 父对象
     */
    DrdRdpListener(const QString &bindAddress,
                   quint16 port,
                   DrdServerRuntime *runtime,
                   const DrdEncodingOptions *encodingOptions,
                   bool nlaEnabled,
                   const QString &nlaUsername,
                   const QString &nlaPassword,
                   const QString &pamService,
                   DrdRuntimeMode runtimeMode,
                   QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdRdpListener() override;

    /**
     * @brief 启动监听器
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool start(QString *error = nullptr);

    /**
     * @brief 停止监听器
     */
    void stop();

    /**
     * @brief 获取运行时对象
     */
    DrdServerRuntime *runtime() const { return m_runtime; }

    /**
     * @brief 是否为 handover 模式
     */
    bool isHandoverMode() const { return m_runtimeMode == DrdRuntimeMode::Handover; }

    /**
     * @brief 是否为单次登录模式
     */
    bool isSingleLogin() const { return m_isSingleLogin; }

private slots:
    /**
     * @brief 处理传入的连接
     * @param server TCP 服务器
     */
    void handleIncomingConnection(QTcpServer *server);

private:
    QString m_bindAddress;
    quint16 m_port;
    DrdServerRuntime *m_runtime;
    DrdEncodingOptions m_encodingOptions;
    bool m_nlaEnabled;
    QString m_nlaUsername;
    QString m_nlaPassword;
    QString m_pamService;
    DrdRuntimeMode m_runtimeMode;
    bool m_isSingleLogin;
};