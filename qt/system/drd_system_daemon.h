#pragma once

#include <QObject>
#include <QString>

// 前向声明
class DrdConfig;
class DrdServerRuntime;
class DrdTlsCredentials;

/**
 * @brief Qt 版本的 DrdSystemDaemon 类
 * 
 * 替代 GObject 版本的 DrdSystemDaemon，使用 Qt 的对象系统
 */
class DrdSystemDaemon : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param config 配置对象
     * @param runtime 运行时对象
     * @param tlsCredentials TLS 凭据
     * @param parent 父对象
     */
    DrdSystemDaemon(DrdConfig *config,
                    DrdServerRuntime *runtime,
                    DrdTlsCredentials *tlsCredentials,
                    QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdSystemDaemon() override;

    /**
     * @brief 启动守护进程
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool start(QString *error = nullptr);

    /**
     * @brief 停止守护进程
     */
    void stop();

    /**
     * @brief 获取待处理客户端数量
     */
    unsigned int pendingClientCount() const { return m_pendingClientCount; }

    /**
     * @brief 获取远程客户端数量
     */
    unsigned int remoteClientCount() const { return m_remoteClientCount; }

private:
    DrdConfig *m_config;
    DrdServerRuntime *m_runtime;
    DrdTlsCredentials *m_tlsCredentials;
    unsigned int m_pendingClientCount;
    unsigned int m_remoteClientCount;
};