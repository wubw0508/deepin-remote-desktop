#pragma once

#include <QObject>
#include <QString>

// 前向声明 FreeRDP 类型（使用 typedef 而不是 struct）
typedef struct rdp_settings rdpSettings;
typedef struct rdp_certificate rdpCertificate;
typedef struct rdp_private_key rdpPrivateKey;

/**
 * @brief Qt 版本的 DrdTlsCredentials 类
 * 
 * 替代 GObject 版本的 DrdTlsCredentials，使用 Qt 的对象系统
 */
class DrdTlsCredentials : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit DrdTlsCredentials(QObject *parent = nullptr);

    /**
     * @brief 从证书和私钥路径创建
     * @param certificatePath 证书路径
     * @param privateKeyPath 私钥路径
     * @param parent 父对象
     */
    DrdTlsCredentials(const QString &certificatePath, const QString &privateKeyPath, QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DrdTlsCredentials() override;

    /**
     * @brief 获取证书路径
     */
    QString certificatePath() const { return m_certificatePath; }

    /**
     * @brief 获取私钥路径
     */
    QString privateKeyPath() const { return m_privateKeyPath; }

    /**
     * @brief 应用 TLS 凭据到设置
     * @param settings FreeRDP 设置
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool apply(rdpSettings *settings, QString *error = nullptr);

    /**
     * @brief 获取证书对象
     */
    rdpCertificate *certificate() const { return m_certificate; }

    /**
     * @brief 获取私钥对象
     */
    rdpPrivateKey *privateKey() const { return m_privateKey; }

    /**
     * @brief 读取证书和密钥材料
     * @param certificate 输出证书内容
     * @param key 输出密钥内容
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool readMaterial(QString *certificate, QString *key, QString *error = nullptr);

    /**
     * @brief 从 PEM 重新加载
     * @param certificatePem 证书 PEM 内容
     * @param keyPem 密钥 PEM 内容
     * @param error 错误输出
     * @return 成功返回 true
     */
    bool reloadFromPem(const QString &certificatePem, const QString &keyPem, QString *error = nullptr);

private:
    QString m_certificatePath;
    QString m_privateKeyPath;
    rdpCertificate *m_certificate;
    rdpPrivateKey *m_privateKey;
};