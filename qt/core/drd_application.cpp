#include "drd_application.h"
#include "drd_config.h"
#include "drd_server_runtime.h"
#include "security/drd_tls_credentials.h"
#include "system/drd_system_daemon.h"
#include "system/drd_handover_daemon.h"
#include "transport/drd_transport.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <csignal>

DrdQtApplication::DrdQtApplication(QObject *parent) : QObject(parent) {}

// 信号处理函数
static QEventLoop* g_main_loop = nullptr;

static void signal_handler(int signal) {
    Q_UNUSED(signal);
    if (g_main_loop && g_main_loop->isRunning()) {
        qDebug() << "Termination signal received, exiting main loop";
        g_main_loop->quit();
    }
}

int DrdQtApplication::drd_application_run(const QStringList &arguments,
                                          QString *error_message) {
    // 初始化配置
    QSharedPointer<DrdQtConfig> config(new DrdQtConfig());
    // 初始化运行时
    QSharedPointer<DrdQtServerRuntime> runtime(new DrdQtServerRuntime());
    // 初始化 TLS 凭据
    QSharedPointer<DrdQtTlsCredentials> tls_credentials(new DrdQtTlsCredentials());

    // 解析命令行参数
    QCommandLineParser parser;
    parser.setApplicationDescription("Deepin Remote Desktop Server");
    parser.addHelpOption();
    parser.addVersionOption();

    // 服务器选项
    QCommandLineOption bindAddressOption("bind-address", "Bind address (default 0.0.0.0)", "ADDR", "0.0.0.0");
    QCommandLineOption portOption("port", "Bind port (default 3390)", "PORT", "3390");
    QCommandLineOption configOption("config", "Configuration file path (ini)", "FILE");
    QCommandLineOption certOption("cert", "TLS certificate PEM path", "FILE");
    QCommandLineOption keyOption("key", "TLS private key PEM path", "FILE");

    // 编码选项
    QCommandLineOption widthOption("width", "Capture width override", "PX");
    QCommandLineOption heightOption("height", "Capture height override", "PX");
    QCommandLineOption captureFpsOption("capture-fps", "Capture target fps", "FPS");
    QCommandLineOption captureStatsIntervalOption("capture-stats-sec", "Capture/render fps stats window seconds", "SEC");
    QCommandLineOption encoderOption("encoder", "Encoder mode (h264|rfx|auto)", "MODE");

    // NLA 选项
    QCommandLineOption nlaUsernameOption("nla-username", "NLA username for static mode", "USER");
    QCommandLineOption nlaPasswordOption("nla-password", "NLA password for static mode", "PASS");
    QCommandLineOption enableNlaOption("enable-nla", "Force enable NLA regardless of config");
    QCommandLineOption disableNlaOption("disable-nla", "Disable NLA and use TLS+PAM single sign-on (system mode only)");

    // 运行模式选项
    QCommandLineOption modeOption("mode", "Runtime mode (user|system|handover)", "MODE", "user");

    // 差分编码选项
    QCommandLineOption enableDiffOption("enable-diff", "Enable frame difference even if disabled in config");
    QCommandLineOption disableDiffOption("disable-diff", "Disable frame difference regardless of config");

    parser.addOption(bindAddressOption);
    parser.addOption(portOption);
    parser.addOption(configOption);
    parser.addOption(certOption);
    parser.addOption(keyOption);
    parser.addOption(widthOption);
    parser.addOption(heightOption);
    parser.addOption(captureFpsOption);
    parser.addOption(captureStatsIntervalOption);
    parser.addOption(encoderOption);
    parser.addOption(nlaUsernameOption);
    parser.addOption(nlaPasswordOption);
    parser.addOption(enableNlaOption);
    parser.addOption(disableNlaOption);
    parser.addOption(modeOption);
    parser.addOption(enableDiffOption);
    parser.addOption(disableDiffOption);

    // 解析参数
    parser.process(arguments);

    // 加载配置文件
    if (parser.isSet(configOption)) {
        if (!config->drd_config_load_from_file(parser.value(configOption), error_message)) {
            return 1;
        }
    }

    // 合并命令行参数到配置
    QVariantMap cli_values;
    if (parser.isSet(bindAddressOption)) {
        cli_values["bind_address"] = parser.value(bindAddressOption);
    }
    if (parser.isSet(portOption)) {
        bool ok;
        int port = parser.value(portOption).toInt(&ok);
        if (ok) {
            cli_values["port"] = port;
        }
    }
    if (parser.isSet(certOption)) {
        cli_values["certificate_path"] = parser.value(certOption);
    }
    if (parser.isSet(keyOption)) {
        cli_values["private_key_path"] = parser.value(keyOption);
    }
    if (parser.isSet(widthOption)) {
        bool ok;
        int width = parser.value(widthOption).toInt(&ok);
        if (ok) {
            cli_values["capture_width"] = width;
        }
    }
    if (parser.isSet(heightOption)) {
        bool ok;
        int height = parser.value(heightOption).toInt(&ok);
        if (ok) {
            cli_values["capture_height"] = height;
        }
    }
    if (parser.isSet(captureFpsOption)) {
        bool ok;
        int fps = parser.value(captureFpsOption).toInt(&ok);
        if (ok) {
            cli_values["capture_target_fps"] = fps;
        }
    }
    if (parser.isSet(captureStatsIntervalOption)) {
        bool ok;
        int interval = parser.value(captureStatsIntervalOption).toInt(&ok);
        if (ok) {
            cli_values["capture_stats_interval_sec"] = interval;
        }
    }
    if (parser.isSet(encoderOption)) {
        cli_values["encoder_mode"] = parser.value(encoderOption);
    }
    if (parser.isSet(nlaUsernameOption)) {
        cli_values["nla_username"] = parser.value(nlaUsernameOption);
    }
    if (parser.isSet(nlaPasswordOption)) {
        cli_values["nla_password"] = parser.value(nlaPasswordOption);
    }
    if (parser.isSet(enableNlaOption)) {
        cli_values["enable_nla"] = true;
    }
    if (parser.isSet(disableNlaOption)) {
        cli_values["disable_nla"] = true;
    }
    if (parser.isSet(modeOption)) {
        cli_values["runtime_mode"] = parser.value(modeOption);
    }
    if (parser.isSet(enableDiffOption)) {
        cli_values["enable_diff"] = true;
    }
    if (parser.isSet(disableDiffOption)) {
        cli_values["disable_diff"] = true;
    }

    if (!config->drd_config_merge_cli(cli_values, error_message)) {
        return 1;
    }

    // 初始化信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 创建主循环
    QEventLoop loop;
    g_main_loop = &loop;

    // 根据运行模式启动服务
    QString runtime_mode = config->drd_config_get_string("runtime_mode");
    bool started = false;

    if (runtime_mode == "system") {
        // 启动系统守护进程
        QSharedPointer<DrdQtSystemDaemon> system_daemon(new DrdQtSystemDaemon(config.data(), runtime.data(), tls_credentials.data()));
        system_daemon->set_main_loop(&loop);
        if (system_daemon->start(error_message)) {
            qDebug() << "System daemon exposed DBus dispatcher (system)";
            started = true;
        }
    } else if (runtime_mode == "handover") {
        // 启动交接守护进程
        QSharedPointer<DrdQtHandoverDaemon> handover_daemon(new DrdQtHandoverDaemon(config.data(), runtime.data(), tls_credentials.data()));
        handover_daemon->set_main_loop(&loop);
        if (handover_daemon->start(error_message)) {
            qDebug() << "Handover daemon initialized (mode=handover)";
            started = true;
        }
    } else {
        // 启动 RDP 监听器 (默认 user 模式)
            auto *transport = new DrdQtTransport();
            QVariantMap encoding_options;
            encoding_options["mode"] = config->drd_config_get_string("encoding_mode");
            encoding_options["enable_diff"] = config->drd_config_get_bool("enable_frame_diff");
            encoding_options["width"] = config->drd_config_get_int("capture_width");
            encoding_options["height"] = config->drd_config_get_int("capture_height");
            
            // 加载 TLS 证书和私钥
            if (!tls_credentials->drd_tls_credentials_load(
                    config->drd_config_get_string("certificate_path"),
                    config->drd_config_get_string("private_key_path"),
                    error_message)) {
                return 1;
            }
            
            // 将 TLS 凭据设置到运行时
            runtime->setTlsCredentials(tls_credentials.data());
            
            QObject *listener = transport->drd_rdp_listener_new(
                config->drd_config_get_string("bind_address"),
                config->drd_config_get_int("port"),
                runtime.data(),
                encoding_options,
                config->drd_config_get_bool("nla_enabled"),
                config->drd_config_get_string("nla_username"),
                config->drd_config_get_string("nla_password"),
                config->drd_config_get_string("pam_service"),
                config->drd_config_get_string("runtime_mode")
            );
            
            if (transport->drd_rdp_listener_start(listener, error_message)) {
                qDebug() << "RDP service listening on" << config->drd_config_get_string("bind_address") << ":" << config->drd_config_get_int("port");
                qDebug() << "Loaded TLS credentials (cert=" << config->drd_config_get_string("certificate_path") << ", key=" << config->drd_config_get_string("private_key_path") << ")";
                started = true;
        }
    }

    if (!started) {
        g_main_loop = nullptr;
        return 1;
    }

    // 运行主循环
    loop.exec();

    g_main_loop = nullptr;
    qDebug() << "Main loop terminated";
    return 0;
}
