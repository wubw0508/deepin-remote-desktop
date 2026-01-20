#include "core/drd_application.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QSocketNotifier>
#include <QTimer>
#include <QDebug>
#include <unistd.h>
#include <sys/types.h>
#include <winpr/ssl.h>

#include "core/drd_config.h"
#include "core/drd_encoding_options.h"
#include "core/drd_server_runtime.h"
#include "security/drd_tls_credentials.h"
#include "transport/drd_rdp_listener.h"
#include "system/drd_system_daemon.h"
#include "system/drd_handover_daemon.h"
#include "utils/drd_log.h"

/**
 * @brief 运行时上下文快照结构体
 */
struct DrdRuntimeContextSnapshot
{
    DrdEncodingOptions encodingOpts;
    bool nlaEnabled;
    QString nlaUsername;
    QString nlaPassword;
    QString pamService;
    DrdRuntimeMode runtimeMode;
};

/**
 * @brief 构造函数
 * 
 * 功能：初始化应用实例默认状态。
 * 逻辑：创建配置与运行时对象，初始化成员变量。
 * 参数：parent 父对象。
 * 外部接口：Qt QObject 构造函数。
 */
DrdApplication::DrdApplication(QObject *parent)
    : QObject(parent)
    , m_config(new DrdConfig(this))
    , m_runtime(new DrdServerRuntime(this))
    , m_tlsCredentials(nullptr)
    , m_listener(nullptr)
    , m_modeController(nullptr)
    , m_isHandover(false)
    , m_qtApp(qobject_cast<QCoreApplication*>(parent))
{
}

/**
 * @brief 析构函数
 * 
 * 功能：清理应用持有的资源。
 * 逻辑：Qt 会自动清理子对象，这里可以添加额外的清理逻辑。
 * 参数：无。
 * 外部接口：Qt QObject 析构函数。
 */
DrdApplication::~DrdApplication()
{
    // Qt 会自动清理子对象
    // 如果需要手动清理，可以在这里添加
}

/**
 * @brief 将运行模式转换为字符串
 * 
 * 功能：将运行模式转为字符串。
 * 逻辑：根据枚举返回 system/handover/user 文本，默认 user。
 * 参数：mode 运行模式。
 * 外部接口：无额外外部库调用。
 */
QString DrdApplication::runtimeModeToString(int mode)
{
    switch (static_cast<DrdRuntimeMode>(mode))
    {
        case DrdRuntimeMode::System:
            return "system";
        case DrdRuntimeMode::Handover:
            return "handover";
        case DrdRuntimeMode::User:
        default:
            return "user";
    }
}

/**
 * @brief 记录生效的配置
 * 
 * 功能：输出当前生效的编码与运行模式配置。
 * 逻辑：读取配置中的编码选项与运行模式，分别记录分辨率、编码模式、差分开关、NLA 与 PAM 服务名。
 * 参数：无。
 * 外部接口：依赖 m_config 获取配置，日志通过 DRD_LOG_MESSAGE。
 */
void DrdApplication::logEffectiveConfig()
{
    if (m_config == nullptr)
    {
        return;
    }

    const DrdEncodingOptions *encodingOpts = m_config->encodingOptions();
    if (encodingOpts)
    {
        DRD_LOG_MESSAGE("Effective capture geometry %ux%u, encoder=%s, frame diff %s",
                        encodingOpts->width,
                        encodingOpts->height,
                        drdEncodingModeToString(encodingOpts->mode).toUtf8().constData(),
                        encodingOpts->enableFrameDiff ? "enabled" : "disabled");
    }

    // NLA 和 PAM 已禁用，只使用 TLS 认证
    DRD_LOG_MESSAGE("Effective authentication: TLS only, runtime=%s",
                    runtimeModeToString(static_cast<int>(m_config->runtimeMode())).toUtf8().constData());
}

/**
 * @brief 处理 SIGINT 信号
 * 
 * 功能：Unix 信号回调，触发主循环退出。
 * 逻辑：若主循环运行中且非 handover 模式则立刻退出；handover 模式下延迟 5 秒退出并记录日志。
 * 参数：无。
 * 外部接口：Qt QCoreApplication::quit，DRD_LOG_MESSAGE。
 */
void DrdApplication::handleSigInt()
{
    if (!m_isHandover || getuid() >= 1000)
    {
        if (m_qtApp)
        {
            m_qtApp->quit();
        }
    }
    else
    {
        DRD_LOG_MESSAGE("Termination signal received, shutting down main loop");
        QTimer::singleShot(5000, this, [this]() {
            if (m_qtApp)
            {
                m_qtApp->quit();
            }
        });
    }
}

/**
 * @brief 处理 SIGTERM 信号
 * 
 * 功能：Unix 信号回调，触发主循环退出。
 * 逻辑：与 handleSigInt 相同。
 * 参数：无。
 * 外部接口：Qt QCoreApplication::quit，DRD_LOG_MESSAGE。
 */
void DrdApplication::handleSigTerm()
{
    handleSigInt();
}

/**
 * @brief 准备运行时上下文
 * 
 * 功能：准备运行时依赖（配置、TLS、编码选项）并可选记录快照。
 * 逻辑：加载/创建配置；校验 TLS 路径与 NLA/PAM 账户；按模式创建或加载 TLS 凭据并注入 runtime；提取编码选项写入 runtime。
 * 参数：error 错误输出。
 * 外部接口：调用 m_config 读取配置、创建 TLS 凭据、注入运行时；错误通过 QString 返回。
 */
bool DrdApplication::prepareRuntime(QString *error)
{
    if (m_config == nullptr)
    {
        if (error)
        {
            *error = "Config is not initialized";
        }
        return false;
    }

    // NLA 和 PAM 已禁用，只使用 TLS 认证
    /*
    bool nlaEnabled = m_config->isNlaEnabled();
    QString nlaUsername = m_config->nlaUsername();
    QString nlaPassword = m_config->nlaPassword();
    QString pamService = m_config->pamService();
    */
    DrdRuntimeMode runtimeMode = m_config->runtimeMode();

    // 验证 TLS 路径
    const bool requireTlsPaths = runtimeMode != DrdRuntimeMode::Handover;
    QString certPath, keyPath;
    if (requireTlsPaths)
    {
        certPath = m_config->certificatePath();
        keyPath = m_config->privateKeyPath();
        if (certPath.isEmpty() || keyPath.isEmpty())
        {
            if (error)
            {
                *error = "TLS certificate or key path missing after config merge";
            }
            return false;
        }
    }

    // NLA 和 PAM 验证已禁用
    /*
    // 验证 NLA 和 PAM 配置
    if (nlaEnabled && runtimeMode != DrdRuntimeMode::Handover &&
        (nlaUsername.isEmpty() || nlaPassword.isEmpty()))
    {
        if (error)
        {
            *error = "NLA username/password missing after config merge";
        }
        return false;
    }

    if (!nlaEnabled && pamService.isEmpty())
    {
        if (error)
        {
            *error = "PAM service missing for TLS authentication";
        }
        return false;
    }
    */

    // 创建 TLS 凭据
    if (m_tlsCredentials == nullptr)
    {
        if (runtimeMode == DrdRuntimeMode::Handover)
        {
            m_tlsCredentials = new DrdTlsCredentials(this);
        }
        else
        {
            m_tlsCredentials = new DrdTlsCredentials(certPath, keyPath, this);
        }
        if (m_tlsCredentials == nullptr)
        {
            if (error && requireTlsPaths)
            {
                *error = "Failed to load TLS credentials";
            }
            return false;
        }
        m_runtime->setTlsCredentials(m_tlsCredentials);
    }

    const DrdEncodingOptions *configEncodingOpts = m_config->encodingOptions();
    if (configEncodingOpts == nullptr)
    {
        if (error)
        {
            *error = "Encoding options unavailable after config merge";
        }
        return false;
    }

    DrdEncodingOptions encodingOpts = *configEncodingOpts;
    m_runtime->setEncodingOptions(&encodingOpts);

    DRD_LOG_MESSAGE("Runtime initialized without capture/encoding setup "
                    "(runtime mode=%s, TLS only authentication)",
                    runtimeModeToString(static_cast<int>(runtimeMode)).toUtf8().constData());

    return true;
}

/**
 * @brief 启动 RDP 监听器（user 模式）
 * 
 * 功能：在 user 模式下启动 RDP 监听器。
 * 逻辑：准备运行时上下文；创建监听器并绑定编码/NLA/TLS 参数；启动监听失败则清理 runtime。
 * 参数：error 错误输出。
 * 外部接口：prepareRuntime、创建 DrdRdpListener、启动监听。
 */
bool DrdApplication::startListener(QString *error)
{
    if (m_listener != nullptr)
    {
        if (error)
        {
            *error = "Listener already started";
        }
        return false;
    }

    if (m_runtime == nullptr)
    {
        if (error)
        {
            *error = "Runtime is not initialized";
        }
        return false;
    }

    DrdRuntimeContextSnapshot snapshot;
    if (!prepareRuntime(error))
    {
        return false;
    }

    snapshot.encodingOpts = *m_config->encodingOptions();
    snapshot.nlaEnabled = m_config->isNlaEnabled();
    snapshot.nlaUsername = m_config->nlaUsername();
    snapshot.nlaPassword = m_config->nlaPassword();
    snapshot.pamService = m_config->pamService();
    snapshot.runtimeMode = m_config->runtimeMode();

    m_listener = new DrdRdpListener(m_config->bindAddress(),
                                     m_config->port(),
                                     m_runtime,
                                     &snapshot.encodingOpts,
                                     snapshot.nlaEnabled,
                                     snapshot.nlaUsername,
                                     snapshot.nlaPassword,
                                     snapshot.pamService,
                                     snapshot.runtimeMode,
                                     this);
    if (!m_listener->start(error))
    {
        delete m_listener;
        m_listener = nullptr;
        return false;
    }

    return true;
}

/**
 * @brief 启动系统守护进程（system 模式）
 * 
 * 功能：在 system 模式下启动守护进程控制器。
 * 逻辑：准备运行时上下文；创建 system 守护并绑定主循环；启动失败则清理控制器。
 * 参数：error 错误输出。
 * 外部接口：prepareRuntime、创建 DrdSystemDaemon、启动守护进程。
 */
bool DrdApplication::startSystemDaemon(QString *error)
{
    DrdRuntimeContextSnapshot snapshot;
    if (!prepareRuntime(error))
    {
        return false;
    }

    // 清理旧的控制器
    if (m_modeController != nullptr)
    {
        delete m_modeController;
        m_modeController = nullptr;
    }

    m_modeController = new DrdSystemDaemon(m_config, m_runtime, m_tlsCredentials, this);
    if (!static_cast<DrdSystemDaemon*>(m_modeController)->start(error))
    {
        delete m_modeController;
        m_modeController = nullptr;
        return false;
    }

    return true;
}

/**
 * @brief 启动交接守护进程（handover 模式）
 * 
 * 功能：在 handover 模式下启动交接守护进程。
 * 逻辑：准备运行时上下文；创建 handover 守护并挂接主循环；启动失败则释放控制器。
 * 参数：error 错误输出。
 * 外部接口：prepareRuntime、创建 DrdHandoverDaemon、启动守护进程。
 */
bool DrdApplication::startHandoverDaemon(QString *error)
{
    DrdRuntimeContextSnapshot snapshot;
    if (!prepareRuntime(error))
    {
        return false;
    }

    // 清理旧的控制器
    if (m_modeController != nullptr)
    {
        delete m_modeController;
        m_modeController = nullptr;
    }

    m_modeController = new DrdHandoverDaemon(m_config, m_runtime, m_tlsCredentials, this);
    if (!static_cast<DrdHandoverDaemon*>(m_modeController)->start(error))
    {
        delete m_modeController;
        m_modeController = nullptr;
        return false;
    }

    return true;
}

/**
 * @brief 解析命令行选项
 * 
 * 功能：解析命令行参数并与配置文件合并。
 * 逻辑：构建 QCommandLineParser 解析 CLI；防止互斥选项组合；加载配置文件或使用默认配置；按 CLI 覆盖配置项并校验权限（system 模式需 root）。
 * 参数：argc 命令行参数数量，argv 命令行参数数组，error 错误输出。
 * 外部接口：Qt QCommandLineParser 解析选项；m_config 读取与合并；geteuid 校验权限；日志 DRD_LOG_MESSAGE。
 */
bool DrdApplication::parseOptions(int argc, char **argv, QString *error)
{
    (void)argc;
    (void)argv;
    
    QCommandLineParser parser;
    parser.setApplicationDescription("Qt FreeRDP minimal server skeleton");

    // 添加命令行选项
    QCommandLineOption bindAddressOption(QStringList() << "b" << "bind-address",
                                        "Bind address (default 0.0.0.0)", "ADDR");
    parser.addOption(bindAddressOption);

    QCommandLineOption portOption(QStringList() << "p" << "port",
                                  "Bind port (default 3390 unless config overrides)", "PORT");
    parser.addOption(portOption);

    QCommandLineOption certOption("cert",
                                  "TLS certificate PEM path", "FILE");
    parser.addOption(certOption);

    QCommandLineOption keyOption("key",
                                 "TLS private key PEM path", "FILE");
    parser.addOption(keyOption);

    QCommandLineOption configOption(QStringList() << "c" << "config",
                                    "Configuration file path (ini)", "FILE");
    parser.addOption(configOption);

    QCommandLineOption widthOption("width",
                                   "Capture width override", "PX");
    parser.addOption(widthOption);

    QCommandLineOption heightOption("height",
                                    "Capture height override", "PX");
    parser.addOption(heightOption);

    QCommandLineOption captureFpsOption("capture-fps",
                                        "Capture target fps", "FPS");
    parser.addOption(captureFpsOption);

    QCommandLineOption captureStatsSecOption("capture-stats-sec",
                                             "Capture/render fps stats window seconds", "SEC");
    parser.addOption(captureStatsSecOption);

    QCommandLineOption encoderOption("encoder",
                                     "Encoder mode (h264|rfx|auto)", "MODE");
    parser.addOption(encoderOption);

    QCommandLineOption nlaUsernameOption("nla-username",
                                         "NLA username for static mode", "USER");
    parser.addOption(nlaUsernameOption);

    QCommandLineOption nlaPasswordOption("nla-password",
                                         "NLA password for static mode", "PASS");
    parser.addOption(nlaPasswordOption);

    QCommandLineOption enableNlaOption("enable-nla",
                                       "Force enable NLA regardless of config");
    parser.addOption(enableNlaOption);

    QCommandLineOption disableNlaOption("disable-nla",
                                        "Disable NLA and use TLS+PAM single sign-on (system mode only)");
    parser.addOption(disableNlaOption);

    QCommandLineOption modeOption("mode",
                                  "Runtime mode (user|system|handover)", "MODE");
    parser.addOption(modeOption);

    QCommandLineOption enableDiffOption("enable-diff",
                                        "Enable frame difference even if disabled in config");
    parser.addOption(enableDiffOption);

    QCommandLineOption disableDiffOption("disable-diff",
                                         "Disable frame difference regardless of config");
    parser.addOption(disableDiffOption);

    parser.process(*m_qtApp);

    // 检查互斥选项
    if (parser.isSet(enableDiffOption) && parser.isSet(disableDiffOption))
    {
        if (error)
        {
            *error = "--enable-diff and --disable-diff cannot be used together";
        }
        return false;
    }

    if (parser.isSet(enableNlaOption) && parser.isSet(disableNlaOption))
    {
        if (error)
        {
            *error = "--enable-nla and --disable-nla cannot be used together";
        }
        return false;
    }

    // 加载配置文件（如果指定）
    if (parser.isSet("config"))
    {
        QString configPath = parser.value("config");
        if (!m_config->loadFromFile(configPath))
        {
            if (error)
            {
                *error = QString("Failed to load config file: %1").arg(configPath);
            }
            return false;
        }
    }

    // 合并命令行选项到配置
    if (!m_config->mergeCommandLineOptions(parser, error))
    {
        return false;
    }

    // 检查权限
    if (m_config->runtimeMode() == DrdRuntimeMode::System && geteuid() != 0)
    {
        if (error)
        {
            *error = "--system requires root privileges";
        }
        return false;
    }

    return true;
}

/**
 * @brief 运行应用程序
 * 
 * 功能：应用入口，负责解析参数、启动相应模式并运行主循环。
 * 逻辑：先解析 CLI；输出生效配置；注册 SIGINT/SIGTERM；按运行模式启动监听器或守护；运行主循环并返回退出码。
 * 参数：argc 命令行参数数量，argv 命令行参数数组，error 错误输出。
 * 外部接口：parseOptions、logEffectiveConfig、QSocketNotifier 注册信号；startListener/startSystemDaemon/startHandoverDaemon 启动子模块；QCoreApplication::exec 运行主循环；日志 DRD_LOG_MESSAGE。
 */
int DrdApplication::run(int argc, char **argv, QString *error)
{
    if (!parseOptions(argc, argv, error))
    {
        return EXIT_FAILURE;
    }

    logEffectiveConfig();

    // 注册 Unix 信号处理
    // 注意：Qt 没有直接的 Unix 信号处理，需要使用 QSocketNotifier
    // 这里简化处理，实际实现需要使用 socketpair + QSocketNotifier
    // 或者使用信号处理函数 + Qt::QueuedConnection

    // 获取运行模式
    DrdRuntimeMode runtimeMode = m_config->runtimeMode();

    bool started = false;
    switch (runtimeMode)
    {
        case DrdRuntimeMode::System:
            started = startSystemDaemon(error);
            if (started)
            {
                DRD_LOG_MESSAGE("System daemon exposing DBus dispatcher (%s)",
                                runtimeModeToString(static_cast<int>(runtimeMode)).toUtf8().constData());
            }
            break;
        case DrdRuntimeMode::Handover:
            m_isHandover = true;
            started = startHandoverDaemon(error);
            if (started)
            {
                DRD_LOG_MESSAGE("Handover daemon initialized (mode=%s)",
                                runtimeModeToString(static_cast<int>(runtimeMode)).toUtf8().constData());
            }
            break;
        case DrdRuntimeMode::User:
        default:
            started = startListener(error);
            if (started)
            {
                DRD_LOG_MESSAGE("RDP service listening on %s:%u",
                                m_config->bindAddress().toUtf8().constData(),
                                m_config->port());
                DRD_LOG_MESSAGE("Loaded TLS credentials (cert=%s, key=%s)",
                                m_config->certificatePath().toUtf8().constData(),
                                m_config->privateKeyPath().toUtf8().constData());
            }
            break;
    }

    if (!started)
    {
        return EXIT_FAILURE;
    }

    // 运行 Qt 事件循环
    if (m_qtApp)
    {
        m_qtApp->exec();
    }

    DRD_LOG_MESSAGE("Main loop terminated");
    return EXIT_SUCCESS;
}