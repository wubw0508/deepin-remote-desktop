#pragma once

#include <QObject>
#include <QString>
#include <QCommandLineParser>
#include <QCoreApplication>

// 前向声明
class DrdConfig;
class DrdServerRuntime;
class DrdTlsCredentials;
class DrdRdpListener;
class DrdSystemDaemon;
class DrdHandoverDaemon;

/**
 * @brief Qt 版本的 DrdApplication 类
 * 
 * 替代 GObject 版本的 DrdApplication，使用 Qt 的对象系统和事件循环
 */
class DrdApplication : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit DrdApplication(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdApplication() override;

    /**
     * @brief 运行应用程序
     * @param argc 命令行参数数量
     * @param argv 命令行参数数组
     * @param error 错误输出
     * @return 退出状态码
     */
    int run(int argc, char **argv, QString *error = nullptr);

    /**
     * @brief 获取配置对象
     * @return 配置对象指针
     */
    DrdConfig *config() const { return m_config; }

    /**
     * @brief 获取运行时对象
     * @return 运行时对象指针
     */
    DrdServerRuntime *runtime() const { return m_runtime; }

    /**
     * @brief 获取 TLS 凭据对象
     * @return TLS 凭据对象指针
     */
    DrdTlsCredentials *tlsCredentials() const { return m_tlsCredentials; }

signals:
    /**
     * @brief 应用程序退出信号
     */
    void aboutToQuit();

private slots:
    /**
     * @brief 处理 SIGINT 信号
     */
    void handleSigInt();

    /**
     * @brief 处理 SIGTERM 信号
     */
    void handleSigTerm();

private:
    /**
     * @brief 解析命令行选项
     * @param argc 命令行参数数量
     * @param argv 命令行参数数组
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool parseOptions(int argc, char **argv, QString *error);

    /**
     * @brief 准备运行时上下文
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool prepareRuntime(QString *error);

    /**
     * @brief 启动 RDP 监听器（user 模式）
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool startListener(QString *error);

    /**
     * @brief 启动系统守护进程（system 模式）
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool startSystemDaemon(QString *error);

    /**
     * @brief 启动交接守护进程（handover 模式）
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool startHandoverDaemon(QString *error);

    /**
     * @brief 记录生效的配置
     */
    void logEffectiveConfig();

    /**
     * @brief 将运行模式转换为字符串
     * @param mode 运行模式
     * @return 模式字符串
     */
    static QString runtimeModeToString(int mode);

private:
    DrdConfig *m_config;
    DrdServerRuntime *m_runtime;
    DrdTlsCredentials *m_tlsCredentials;
    DrdRdpListener *m_listener;
    QObject *m_modeController;
    bool m_isHandover;
    QCoreApplication *m_qtApp;
};