#include "core/drd_config.h"

#include <QCommandLineParser>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>

/**
 * @brief 构造函数
 * 
 * 功能：初始化配置对象，使用默认值。
 * 逻辑：设置所有配置项为默认值。
 * 参数：parent 父对象。
 * 外部接口：Qt QObject 构造函数。
 */
DrdConfig::DrdConfig(QObject *parent)
    : QObject(parent)
    , m_bindAddress("0.0.0.0")
    , m_port(3390)
    , m_nlaEnabled(false)  // 禁用 NLA，只使用 TLS 认证
    , m_runtimeMode(DrdRuntimeMode::User)
    , m_pamService("")  // 禁用 PAM
    , m_pamServiceOverridden(false)
    , m_baseDir(QDir::currentPath())
    , m_captureWidth(1024)
    , m_captureHeight(768)
    , m_captureTargetFps(60)
    , m_captureStatsIntervalSec(5)
    , m_valid(true)
{
    // 初始化编码选项为默认值
    m_encodingOptions.width = m_captureWidth;
    m_encodingOptions.height = m_captureHeight;
    m_encodingOptions.mode = DrdEncodingMode::Rfx;
    m_encodingOptions.enableFrameDiff = true;
    m_encodingOptions.h264Bitrate = 5000000;
    m_encodingOptions.h264Framerate = 60;
    m_encodingOptions.h264Qp = 15;
    m_encodingOptions.h264HwAccel = false;
    m_encodingOptions.h264VmSupport = false;
    m_encodingOptions.gfxLargeChangeThreshold = 0.05;
    m_encodingOptions.gfxProgressiveRefreshInterval = 6;
    m_encodingOptions.gfxProgressiveRefreshTimeoutMs = 100;
    
    refreshPamService();
}

/**
 * @brief 从文件加载配置
 *
 * 功能：从指定文件加载配置。
 * 逻辑：读取 INI 格式的配置文件并解析。
 * 参数：path 配置文件路径，parent 父对象。
 * 外部接口：Qt QSettings 读取配置文件。
 */
DrdConfig::DrdConfig(const QString &path, QObject *parent)
    : DrdConfig(parent)
{
    loadFromFile(path);
}

/**
 * @brief 析构函数
 * 
 * 功能：清理配置对象。
 * 逻辑：Qt 会自动清理子对象。
 * 参数：无。
 * 外部接口：Qt QObject 析构函数。
 */
DrdConfig::~DrdConfig()
{
    // Qt 会自动清理子对象
}

/**
 * @brief 解析布尔字符串
 *
 * 功能：解析布尔字符串为 bool 值。
 * 逻辑：匹配 true/false 的多种大小写与数字表示。
 * 参数：value 输入字符串；out_value 输出布尔值。
 * 外部接口：Qt QString 比较。
 */
static bool parseBool(const QString &value, bool *out_value)
{
    if (value.isEmpty()) {
        return false;
    }
    
    QString lower = value.toLower();
    if (lower == "true" || lower == "yes" || lower == "1") {
        *out_value = true;
        return true;
    }
    if (lower == "false" || lower == "no" || lower == "0") {
        *out_value = false;
        return true;
    }
    
    return false;
}

/**
 * @brief 解析运行模式字符串
 *
 * 功能：解析运行模式字符串为枚举值。
 * 逻辑：匹配 user/system/handover。
 * 参数：value 字符串；out_mode 输出枚举。
 * 外部接口：Qt QString 比较。
 */
static bool parseRuntimeMode(const QString &value, DrdRuntimeMode *out_mode)
{
    if (value.isEmpty()) {
        return false;
    }
    
    QString lower = value.toLower();
    if (lower == "user") {
        *out_mode = DrdRuntimeMode::User;
        return true;
    }
    if (lower == "system") {
        *out_mode = DrdRuntimeMode::System;
        return true;
    }
    if (lower == "handover") {
        *out_mode = DrdRuntimeMode::Handover;
        return true;
    }
    
    return false;
}

/**
 * @brief 从字符串设置编码模式
 *
 * 功能：接受 h264/rfx/remotefx/auto 并写入对应枚举。
 * 参数：value 模式名称。
 * 外部接口：Qt QString 比较。
 */
static bool setModeFromString(const QString &value, DrdEncodingMode *out_mode)
{
    if (value.isEmpty()) {
        return false;
    }
    
    QString lower = value.toLower();
    if (lower == "h264") {
        *out_mode = DrdEncodingMode::H264;
        return true;
    }
    if (lower == "rfx" || lower == "remotefx") {
        *out_mode = DrdEncodingMode::Rfx;
        return true;
    }
    if (lower == "auto") {
        *out_mode = DrdEncodingMode::Auto;
        return true;
    }
    
    return false;
}

/**
 * @brief 根据当前运行模式刷新 PAM 服务名
 *
 * 功能：若未被 CLI/配置覆盖则为 system 模式设置 system 服务名，否则使用默认服务名。
 * 外部接口：无。
 */
void DrdConfig::refreshPamService()
{
    if (m_pamServiceOverridden) {
        return;
    }
    
    if (m_runtimeMode == DrdRuntimeMode::System) {
        m_pamService = "deepin-remote-desktop-system";
    } else {
        m_pamService = "deepin-remote-desktop";
    }
}

/**
 * @brief 覆盖 PAM 服务名
 *
 * 功能：若给定值非空则替换 pam_service 并标记已覆盖。
 * 参数：value 新服务名。
 * 外部接口：无。
 */
void DrdConfig::overridePamService(const QString &value)
{
    if (value.isEmpty()) {
        return;
    }
    m_pamService = value;
    m_pamServiceOverridden = true;
}

/**
 * @brief 解析路径为绝对路径
 *
 * 功能：若路径已绝对直接复制，否则基于 base_dir/current_dir 组合并规范化返回。
 * 参数：value 原始路径。
 * 外部接口：Qt QFileInfo/QDir。
 */
QString DrdConfig::resolvePath(const QString &value) const
{
    if (value.isEmpty()) {
        return QString();
    }
    
    QFileInfo fileInfo(value);
    if (fileInfo.isAbsolute()) {
        return value;
    }
    
    QString base = m_baseDir.isEmpty() ? QDir::currentPath() : m_baseDir;
    QDir dir(base);
    return dir.absoluteFilePath(value);
}

/**
 * @brief 从文件加载配置
 *
 * 功能：从磁盘加载 ini 配置并填充配置对象。
 * 逻辑：读取文件为 QSettings，随后调用 loadFromSettings 填充字段。
 * 参数：path 配置文件路径。
 * 外部接口：Qt QSettings 读取配置文件。
 */
bool DrdConfig::loadFromFile(const QString &path)
{
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        qWarning() << "Config file does not exist:" << path;
        m_valid = false;
        return false;
    }
    
    qInfo() << "Loading config file:" << path;
    
    // 更新 base_dir 为配置文件所在目录
    m_baseDir = fileInfo.absolutePath();
    
    QSettings settings(path, QSettings::IniFormat);
    
    return loadFromSettings(settings);
}

/**
 * @brief 从 QSettings 加载配置
 *
 * 功能：从 QSettings 读取配置段并写入实例。
 * 逻辑：解析 server/tls/capture/encoding/auth/service 等段，处理布尔与枚举校验，必要时转换路径或刷新 PAM 服务。
 * 参数：settings QSettings 对象。
 * 外部接口：Qt QSettings API。
 */
bool DrdConfig::loadFromSettings(QSettings &settings)
{
    bool nla_auth_override = false;
    
    // [server] 段
    settings.beginGroup("server");
    if (settings.contains("bind_address")) {
        m_bindAddress = settings.value("bind_address").toString();
        qInfo() << "Config [server] bind_address:" << m_bindAddress;
    }
    if (settings.contains("port")) {
        int port = settings.value("port").toInt();
        if (port > 0 && port <= 65535) {
            m_port = static_cast<quint16>(port);
            qInfo() << "Config [server] port:" << m_port;
        } else {
            qWarning() << "Invalid port value:" << port;
        }
    }
    settings.endGroup();
    
    // [tls] 段
    settings.beginGroup("tls");
    if (settings.contains("certificate")) {
        QString value = settings.value("certificate").toString();
        m_certificatePath = resolvePath(value);
        qInfo() << "Config [tls] certificate:" << m_certificatePath;
    }
    if (settings.contains("private_key")) {
        QString value = settings.value("private_key").toString();
        m_privateKeyPath = resolvePath(value);
        qInfo() << "Config [tls] private_key:" << m_privateKeyPath;
    }
    settings.endGroup();
    
    // [capture] 段
    settings.beginGroup("capture");
    if (settings.contains("width")) {
        int width = settings.value("width").toInt();
        if (width > 0) {
            m_captureWidth = static_cast<unsigned int>(width);
            m_encodingOptions.width = m_captureWidth;
            qInfo() << "Config [capture] width:" << m_captureWidth;
        }
    }
    if (settings.contains("height")) {
        int height = settings.value("height").toInt();
        if (height > 0) {
            m_captureHeight = static_cast<unsigned int>(height);
            m_encodingOptions.height = m_captureHeight;
            qInfo() << "Config [capture] height:" << m_captureHeight;
        }
    }
    if (settings.contains("target_fps")) {
        int target_fps = settings.value("target_fps").toInt();
        if (target_fps > 0) {
            m_captureTargetFps = static_cast<unsigned int>(target_fps);
            qInfo() << "Config [capture] target_fps:" << m_captureTargetFps;
        }
    }
    if (settings.contains("stats_interval_sec")) {
        int stats_interval = settings.value("stats_interval_sec").toInt();
        if (stats_interval > 0) {
            m_captureStatsIntervalSec = static_cast<unsigned int>(stats_interval);
            qInfo() << "Config [capture] stats_interval_sec:" << m_captureStatsIntervalSec;
        }
    }
    settings.endGroup();
    
    // [encoding] 段
    settings.beginGroup("encoding");
    if (settings.contains("mode")) {
        QString mode = settings.value("mode").toString();
        DrdEncodingMode parsed_mode;
        if (setModeFromString(mode, &parsed_mode)) {
            m_encodingOptions.mode = parsed_mode;
            qInfo() << "Config [encoding] mode:" << mode;
        } else {
            qWarning() << "Unknown encoder mode:" << mode;
        }
    }
    if (settings.contains("enable_diff")) {
        QString diff = settings.value("enable_diff").toString();
        bool value = true;
        if (parseBool(diff, &value)) {
            m_encodingOptions.enableFrameDiff = value;
            qInfo() << "Config [encoding] enable_diff:" << (value ? "true" : "false");
        }
    }
    if (settings.contains("h264_bitrate")) {
        int bitrate = settings.value("h264_bitrate").toInt();
        if (bitrate > 0) {
            m_encodingOptions.h264Bitrate = static_cast<unsigned int>(bitrate);
            qInfo() << "Config [encoding] h264_bitrate:" << m_encodingOptions.h264Bitrate;
        }
    }
    if (settings.contains("h264_framerate")) {
        int framerate = settings.value("h264_framerate").toInt();
        if (framerate > 0) {
            m_encodingOptions.h264Framerate = static_cast<unsigned int>(framerate);
            qInfo() << "Config [encoding] h264_framerate:" << m_encodingOptions.h264Framerate;
        }
    }
    if (settings.contains("h264_qp")) {
        int qp = settings.value("h264_qp").toInt();
        if (qp > 0) {
            m_encodingOptions.h264Qp = static_cast<unsigned int>(qp);
            qInfo() << "Config [encoding] h264_qp:" << m_encodingOptions.h264Qp;
        }
    }
    if (settings.contains("h264_hw_accel")) {
        QString hw_accel = settings.value("h264_hw_accel").toString();
        bool value = false;
        if (parseBool(hw_accel, &value)) {
            m_encodingOptions.h264HwAccel = value;
            qInfo() << "Config [encoding] h264_hw_accel:" << (value ? "true" : "false");
        }
    }
    if (settings.contains("h264_vm_support")) {
        QString vm_support = settings.value("h264_vm_support").toString();
        bool value = false;
        if (parseBool(vm_support, &value)) {
            m_encodingOptions.h264VmSupport = value;
            qInfo() << "Config [encoding] h264_vm_support:" << (value ? "true" : "false");
        }
    }
    if (settings.contains("gfx_large_change_threshold")) {
        double threshold = settings.value("gfx_large_change_threshold").toDouble();
        if (threshold >= 0.0) {
            m_encodingOptions.gfxLargeChangeThreshold = threshold;
            qInfo() << "Config [encoding] gfx_large_change_threshold:" << threshold;
        }
    }
    if (settings.contains("gfx_progressive_refresh_interval")) {
        int interval = settings.value("gfx_progressive_refresh_interval").toInt();
        if (interval >= 0) {
            m_encodingOptions.gfxProgressiveRefreshInterval = static_cast<unsigned int>(interval);
            qInfo() << "Config [encoding] gfx_progressive_refresh_interval:" << m_encodingOptions.gfxProgressiveRefreshInterval;
        }
    }
    if (settings.contains("gfx_progressive_refresh_timeout_ms")) {
        int timeout_ms = settings.value("gfx_progressive_refresh_timeout_ms").toInt();
        if (timeout_ms >= 0) {
            m_encodingOptions.gfxProgressiveRefreshTimeoutMs = static_cast<unsigned int>(timeout_ms);
            qInfo() << "Config [encoding] gfx_progressive_refresh_timeout_ms:" << m_encodingOptions.gfxProgressiveRefreshTimeoutMs;
        }
    }
    settings.endGroup();
    
    // [auth] 段 - 已禁用 NLA 和 PAM，只使用 TLS 认证
    settings.beginGroup("auth");
    // NLA 相关配置已禁用
    /*
    if (settings.contains("username")) {
        m_nlaUsername = settings.value("username").toString();
        DRD_LOG_INFO("Config [auth] username: %s", m_nlaUsername.toUtf8().constData());
    }
    if (settings.contains("password")) {
        m_nlaPassword = settings.value("password").toString();
        DRD_LOG_INFO("Config [auth] password: ***");
    }
    if (settings.contains("mode")) {
        QString mode = settings.value("mode").toString();
        if (mode.toLower() == "static") {
            m_nlaEnabled = true;
            DRD_LOG_INFO("Config [auth] mode: static");
        } else {
            qWarning() << "NLA delegate mode has been removed; disable NLA via [auth] enable_nla=false";
        }
    }
    if (settings.contains("enable_nla")) {
        QString nla_value = settings.value("enable_nla").toString();
        bool enable_nla = true;
        if (parseBool(nla_value, &enable_nla)) {
            m_nlaEnabled = enable_nla;
            nla_auth_override = true;
            DRD_LOG_INFO("Config [auth] enable_nla: %s", enable_nla ? "true" : "false");
        }
    } else if (settings.contains("nla")) {
        QString nla_value = settings.value("nla").toString();
        bool enable_nla = true;
        if (parseBool(nla_value, &enable_nla)) {
            m_nlaEnabled = enable_nla;
            nla_auth_override = true;
            DRD_LOG_INFO("Config [auth] nla: %s", enable_nla ? "true" : "false");
        }
    }
    */
    // PAM 相关配置已禁用
    /*
    if (settings.contains("pam_service")) {
        QString pam_service = settings.value("pam_service").toString();
        overridePamService(pam_service);
        DRD_LOG_INFO("Config [auth] pam_service: %s", m_pamService.toUtf8().constData());
    }
    */
    settings.endGroup();
    
    // [service] 段
    settings.beginGroup("service");
    if (settings.contains("runtime_mode")) {
        QString runtime_mode = settings.value("runtime_mode").toString();
        DrdRuntimeMode parsed_mode;
        if (parseRuntimeMode(runtime_mode, &parsed_mode)) {
            setRuntimeMode(parsed_mode);
            qInfo() << "Config [service] runtime_mode:" << runtime_mode;
        }
    } else if (settings.contains("system")) {
        QString system_value = settings.value("system").toString();
        bool system_mode = false;
        if (parseBool(system_value, &system_mode)) {
            setRuntimeMode(system_mode ? DrdRuntimeMode::System : DrdRuntimeMode::User);
            qInfo() << "Config [service] system:" << (system_mode ? "true" : "false");
        }
    }
    
    if (!nla_auth_override && settings.contains("rdp_sso")) {
        QString rdp_sso_str = settings.value("rdp_sso").toString();
        bool rdp_sso = false;
        if (parseBool(rdp_sso_str, &rdp_sso)) {
            m_nlaEnabled = !rdp_sso;
            qInfo() << "Config [service] rdp_sso:" << (rdp_sso ? "true" : "false");
        }
    }
    settings.endGroup();
    
    return true;
}

/**
 * @brief 设置运行模式
 *
 * 功能：设置运行模式并刷新相关配置。
 * 逻辑：在模式变更时更新内部枚举并调用 PAM 服务刷新。
 * 参数：mode 新模式。
 * 外部接口：无。
 */
void DrdConfig::setRuntimeMode(DrdRuntimeMode mode)
{
    if (m_runtimeMode == mode) {
        return;
    }
    
    m_runtimeMode = mode;
    refreshPamService();
}

/**
 * @brief 合并命令行选项
 *
 * 功能：将命令行选项合并到配置中。
 * 逻辑：逐项覆盖监听地址/端口、TLS 路径、NLA 凭据、运行模式、分辨率、编码模式与差分开关；校验互斥条件与必填项；NLA/PAM 约束失败则报错。
 * 参数：parser 命令行解析器，error 错误输出。
 * 外部接口：Qt QCommandLineParser 获取选项值。
 */
bool DrdConfig::mergeCommandLineOptions(const QCommandLineParser &parser, QString *error)
{
    qInfo() << "Merging command line options";
    
    // 绑定地址
    if (parser.isSet("bind-address")) {
        m_bindAddress = parser.value("bind-address");
        qInfo() << "CLI override bind_address:" << m_bindAddress;
    }
    
    // 端口
    if (parser.isSet("port")) {
        bool ok;
        int port = parser.value("port").toInt(&ok);
        if (!ok || port <= 0 || port > 65535) {
            if (error) {
                *error = QString("Invalid port value: %1").arg(parser.value("port"));
            }
            return false;
        }
        m_port = static_cast<quint16>(port);
        qInfo() << "CLI override port:" << m_port;
    }
    
    // 证书路径
    if (parser.isSet("cert")) {
        QString cert_path = parser.value("cert");
        m_certificatePath = resolvePath(cert_path);
        qInfo() << "CLI override certificate:" << m_certificatePath;
    }
    
    // 私钥路径
    if (parser.isSet("key")) {
        QString key_path = parser.value("key");
        m_privateKeyPath = resolvePath(key_path);
        qInfo() << "CLI override private_key:" << m_privateKeyPath;
    }
    
    // NLA 用户名
    if (parser.isSet("nla-username")) {
        m_nlaUsername = parser.value("nla-username");
        qInfo() << "CLI override nla_username:" << m_nlaUsername;
    }
    
    // NLA 密码
    if (parser.isSet("nla-password")) {
        m_nlaPassword = parser.value("nla-password");
        qInfo() << "CLI override nla_password: ***";
    }
    
    // NLA 启用/禁用
    bool cli_enable_nla = parser.isSet("enable-nla");
    bool cli_disable_nla = parser.isSet("disable-nla");
    
    if (cli_enable_nla && cli_disable_nla) {
        if (error) {
            *error = "Cannot enable and disable NLA at the same time";
        }
        return false;
    }
    if (cli_enable_nla) {
        m_nlaEnabled = true;
        qInfo() << "CLI override enable_nla: true";
    } else if (cli_disable_nla) {
        m_nlaEnabled = false;
        qInfo() << "CLI override enable_nla: false";
    }
    
    // 运行模式
    if (parser.isSet("mode")) {
        QString runtime_mode_name = parser.value("mode");
        DrdRuntimeMode cli_mode;
        if (!parseRuntimeMode(runtime_mode_name, &cli_mode)) {
            if (error) {
                *error = QString("Invalid runtime mode: %1 (expected user, system or handover)").arg(runtime_mode_name);
            }
            return false;
        }
        setRuntimeMode(cli_mode);
        qInfo() << "CLI override runtime_mode:" << runtime_mode_name;
    }
    
    // 分辨率
    if (parser.isSet("width")) {
        bool ok;
        int width = parser.value("width").toInt(&ok);
        if (ok && width > 0) {
            m_captureWidth = static_cast<unsigned int>(width);
            m_encodingOptions.width = m_captureWidth;
            qInfo() << "CLI override width:" << m_captureWidth;
        }
    }
    
    if (parser.isSet("height")) {
        bool ok;
        int height = parser.value("height").toInt(&ok);
        if (ok && height > 0) {
            m_captureHeight = static_cast<unsigned int>(height);
            m_encodingOptions.height = m_captureHeight;
            qInfo() << "CLI override height:" << m_captureHeight;
        }
    }
    
    // 编码模式
    if (parser.isSet("encoder")) {
        QString encoder_mode = parser.value("encoder");
        if (!setModeFromString(encoder_mode, &m_encodingOptions.mode)) {
            if (error) {
                *error = QString("Unknown encoder mode: %1 (expected h264, rfx or auto)").arg(encoder_mode);
            }
            return false;
        }
        qInfo() << "CLI override encoder_mode:" << encoder_mode;
    }
    
    // 差分开关
    if (parser.isSet("enable-diff")) {
        m_encodingOptions.enableFrameDiff = true;
        qInfo() << "CLI override enable_diff: true";
    } else if (parser.isSet("disable-diff")) {
        m_encodingOptions.enableFrameDiff = false;
        qInfo() << "CLI override enable_diff: false";
    }
    
    // 捕获目标 FPS
    if (parser.isSet("capture-fps")) {
        bool ok;
        int target_fps = parser.value("capture-fps").toInt(&ok);
        if (ok && target_fps > 0) {
            m_captureTargetFps = static_cast<unsigned int>(target_fps);
            qInfo() << "CLI override capture_fps:" << m_captureTargetFps;
        }
    }
    
    // 捕获统计间隔
    if (parser.isSet("capture-stats-sec")) {
        bool ok;
        int stats_interval = parser.value("capture-stats-sec").toInt(&ok);
        if (ok && stats_interval > 0) {
            m_captureStatsIntervalSec = static_cast<unsigned int>(stats_interval);
            qInfo() << "CLI override capture_stats_sec:" << m_captureStatsIntervalSec;
        }
    }
    
    // 验证配置 - 只验证 TLS 证书
    if (m_runtimeMode != DrdRuntimeMode::Handover &&
        (m_certificatePath.isEmpty() || m_privateKeyPath.isEmpty())) {
        if (error) {
            *error = "TLS certificate and private key must be specified via config or CLI";
        }
        return false;
    }
    
    // NLA 和 PAM 验证已禁用
    /*
    if (!m_nlaEnabled && m_runtimeMode != DrdRuntimeMode::System) {
        if (error) {
            *error = "Disabling NLA requires --system";
        }
        return false;
    }
    
    if (m_nlaEnabled && m_runtimeMode != DrdRuntimeMode::Handover) {
        if (m_nlaUsername.isEmpty() || m_nlaPassword.isEmpty()) {
            if (error) {
                *error = "NLA username and password must be specified via config or CLI";
            }
            return false;
        }
    }
    
    if (m_pamService.isEmpty()) {
        if (error) {
            *error = "PAM service name is not configured";
        }
        return false;
    }
    */
    
    return true;
}