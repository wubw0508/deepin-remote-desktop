#pragma once

#include <QObject>
#include <QString>
#include <QCommandLineParser>
#include <QSettings>

#include "core/drd_encoding_options.h"

/**
 * @brief Qt 版本的 DrdConfig 类
 *
 * 替代 GObject 版本的 DrdConfig，使用 Qt 的对象系统
 */
class DrdConfig : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit DrdConfig(QObject *parent = nullptr);

    /**
     * @brief 从文件加载配置
     * @param path 配置文件路径
     * @param parent 父对象
     */
    explicit DrdConfig(const QString &path, QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdConfig() override;

    /**
     * @brief 获取绑定地址
     */
    QString bindAddress() const { return m_bindAddress; }

    /**
     * @brief 获取端口
     */
    quint16 port() const { return m_port; }

    /**
     * @brief 获取证书路径
     */
    QString certificatePath() const { return m_certificatePath; }

    /**
     * @brief 获取私钥路径
     */
    QString privateKeyPath() const { return m_privateKeyPath; }

    /**
     * @brief 获取 NLA 用户名
     */
    QString nlaUsername() const { return m_nlaUsername; }

    /**
     * @brief 获取 NLA 密码
     */
    QString nlaPassword() const { return m_nlaPassword; }

    /**
     * @brief 是否启用 NLA
     */
    bool isNlaEnabled() const { return m_nlaEnabled; }

    /**
     * @brief 获取运行模式
     */
    DrdRuntimeMode runtimeMode() const { return m_runtimeMode; }

    /**
     * @brief 获取 PAM 服务
     */
    QString pamService() const { return m_pamService; }

    /**
     * @brief 获取捕获宽度
     */
    unsigned int captureWidth() const { return m_captureWidth; }

    /**
     * @brief 获取捕获高度
     */
    unsigned int captureHeight() const { return m_captureHeight; }

    /**
     * @brief 获取捕获目标 FPS
     */
    unsigned int captureTargetFps() const { return m_captureTargetFps; }

    /**
     * @brief 获取捕获统计间隔（秒）
     */
    unsigned int captureStatsIntervalSec() const { return m_captureStatsIntervalSec; }

    /**
     * @brief 获取编码选项
     */
    const DrdEncodingOptions *encodingOptions() const { return &m_encodingOptions; }

    /**
     * @brief 从文件加载配置
     * @param path 配置文件路径
     * @return 成功返回 true
     */
    bool loadFromFile(const QString &path);

    /**
     * @brief 合并命令行选项
     * @param parser 命令行解析器
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool mergeCommandLineOptions(const QCommandLineParser &parser, QString *error = nullptr);

    /**
     * @brief 检查配置是否有效
     */
    bool isValid() const { return m_valid; }

private:
    
    /**
     * @brief 从 QSettings 加载配置
     * @param settings QSettings 对象
     * @return 成功返回 true
     */
    bool loadFromSettings(QSettings &settings);
    
    /**
     * @brief 解析路径为绝对路径
     * @param value 原始路径
     * @return 绝对路径
     */
    QString resolvePath(const QString &value) const;
    
    /**
     * @brief 设置运行模式
     * @param mode 新模式
     */
    void setRuntimeMode(DrdRuntimeMode mode);
    
    /**
     * @brief 刷新 PAM 服务名
     */
    void refreshPamService();
    
    /**
     * @brief 覆盖 PAM 服务名
     * @param value 新服务名
     */
    void overridePamService(const QString &value);

    QString m_bindAddress;
    quint16 m_port;
    QString m_certificatePath;
    QString m_privateKeyPath;
    QString m_nlaUsername;
    QString m_nlaPassword;
    bool m_nlaEnabled;
    DrdRuntimeMode m_runtimeMode;
    QString m_pamService;
    bool m_pamServiceOverridden;
    QString m_baseDir;
    unsigned int m_captureWidth;
    unsigned int m_captureHeight;
    unsigned int m_captureTargetFps;
    unsigned int m_captureStatsIntervalSec;
    DrdEncodingOptions m_encodingOptions;
    bool m_valid;
};