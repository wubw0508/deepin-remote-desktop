#pragma once

#include <QObject>
#include <QString>

// 前向声明
class DrdConfig;
class DrdServerRuntime;
class DrdTlsCredentials;

/**
 * @brief Qt 版本的 DrdHandoverDaemon 类
 * 
 * 替代 GObject 版本的 DrdHandoverDaemon，使用 Qt 的对象系统
 */
class DrdHandoverDaemon : public QObject
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
    DrdHandoverDaemon(DrdConfig *config,
                      DrdServerRuntime *runtime,
                      DrdTlsCredentials *tlsCredentials,
                      QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdHandoverDaemon() override;

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

private:
    DrdConfig *m_config;
    DrdServerRuntime *m_runtime;
    DrdTlsCredentials *m_tlsCredentials;
};