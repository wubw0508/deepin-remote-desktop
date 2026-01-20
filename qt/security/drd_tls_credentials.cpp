#include "security/drd_tls_credentials.h"

#include <QFile>
#include <QDebug>
#include <freerdp/crypto/certificate.h>
#include <freerdp/crypto/privatekey.h>
#include <freerdp/settings.h>
#include <cstring>

/**
 * @brief 构造函数
 * 
 * 功能：初始化 TLS 凭据对象。
 * 逻辑：设置成员变量为默认值。
 * 参数：parent 父对象。
 * 外部接口：Qt QObject 构造函数。
 */
DrdTlsCredentials::DrdTlsCredentials(QObject *parent)
    : QObject(parent)
    , m_certificate(nullptr)
    , m_privateKey(nullptr)
{
}

/**
 * @brief 从证书和私钥路径创建
 * 
 * 功能：从指定路径加载 TLS 凭据。
 * 逻辑：保存证书和私钥路径，加载证书和密钥。
 * 参数：certificatePath 证书路径，privateKeyPath 私钥路径，parent 父对象。
 * 外部接口：Qt QFile 读取文件，FreeRDP API 加载证书。
 */
DrdTlsCredentials::DrdTlsCredentials(const QString &certificatePath, const QString &privateKeyPath, QObject *parent)
    : DrdTlsCredentials(parent)
{
    m_certificatePath = certificatePath;
    m_privateKeyPath = privateKeyPath;

    // 加载证书和密钥
    QString certPem, keyPem;
    if (!readMaterial(&certPem, &keyPem, nullptr))
    {
        qWarning() << "Failed to read TLS material from files";
        return;
    }

    if (!reloadFromPem(certPem, keyPem, nullptr))
    {
        qWarning() << "Failed to parse TLS material";
    }
}

/**
 * @brief 析构函数
 * 
 * 功能：清理 TLS 凭据对象。
 * 逻辑：释放证书和私钥资源。
 * 参数：无。
 * 外部接口：Qt QObject 析构函数，FreeRDP API 释放资源。
 */
DrdTlsCredentials::~DrdTlsCredentials()
{
    if (m_certificate != nullptr)
    {
        freerdp_certificate_free(m_certificate);
        m_certificate = nullptr;
    }
    if (m_privateKey != nullptr)
    {
        freerdp_key_free(m_privateKey);
        m_privateKey = nullptr;
    }
}

/**
 * @brief 应用 TLS 凭据到设置
 * 
 * 功能：将 TLS 凭据应用到 FreeRDP 设置。
 * 逻辑：将证书和私钥设置到 rdpSettings 中。
 * 参数：settings FreeRDP 设置，error 错误输出。
 * 外部接口：FreeRDP API 设置证书和密钥。
 */
bool DrdTlsCredentials::apply(rdpSettings *settings, QString *error)
{
    if (settings == nullptr)
    {
        if (error)
        {
            *error = "Settings is null";
        }
        return false;
    }

    if (m_certificate == nullptr || m_privateKey == nullptr)
    {
        if (error)
        {
            *error = "TLS material unavailable for settings";
        }
        return false;
    }

    // 重新解析证书（因为 FreeRDP 可能需要新的实例）
    rdpCertificate *certificate = freerdp_certificate_new_from_pem(m_certificatePath.toUtf8().constData());
    if (certificate == nullptr)
    {
        if (error)
        {
            *error = "Failed to parse TLS certificate material";
        }
        return false;
    }

    // 重新解析私钥
    rdpPrivateKey *key = freerdp_key_new_from_pem(m_privateKeyPath.toUtf8().constData());
    if (key == nullptr)
    {
        freerdp_certificate_free(certificate);
        if (error)
        {
            *error = "Failed to parse TLS private key material";
        }
        return false;
    }

    // 设置证书到 FreeRDP
    if (!freerdp_settings_set_pointer_len(settings,
                                          FreeRDP_RdpServerCertificate,
                                          certificate, 1))
    {
        freerdp_certificate_free(certificate);
        freerdp_key_free(key);
        if (error)
        {
            *error = "Failed to assign server certificate to settings";
        }
        return false;
    }

    // 设置私钥到 FreeRDP
    if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerRsaKey, key, 1))
    {
        freerdp_key_free(key);
        if (error)
        {
            *error = "Failed to assign private key to settings";
        }
        return false;
    }

    return true;
}

/**
 * @brief 读取证书和密钥材料
 * 
 * 功能：读取证书和密钥的 PEM 内容。
 * 逻辑：从文件读取证书和密钥的 PEM 格式内容。
 * 参数：certificate 输出证书内容，key 输出密钥内容，error 错误输出。
 * 外部接口：Qt QFile 读取文件。
 */
bool DrdTlsCredentials::readMaterial(QString *certificate, QString *key, QString *error)
{
    if (certificate == nullptr || key == nullptr)
    {
        if (error)
        {
            *error = "Output parameters are null";
        }
        return false;
    }

    // 读取证书文件
    QFile certFile(m_certificatePath);
    if (!certFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (error)
        {
            *error = QString("Failed to open certificate file: %1").arg(m_certificatePath);
        }
        return false;
    }
    *certificate = QString::fromUtf8(certFile.readAll());
    certFile.close();

    // 读取私钥文件
    QFile keyFile(m_privateKeyPath);
    if (!keyFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (error)
        {
            *error = QString("Failed to open private key file: %1").arg(m_privateKeyPath);
        }
        return false;
    }
    *key = QString::fromUtf8(keyFile.readAll());
    keyFile.close();

    return true;
}

/**
 * @brief 从 PEM 重新加载
 * 
 * 功能：从 PEM 内容重新加载证书和密钥。
 * 逻辑：解析 PEM 格式的证书和密钥，更新内部对象。
 * 参数：certificatePem 证书 PEM 内容，keyPem 密钥 PEM 内容，error 错误输出。
 * 外部接口：FreeRDP API 解析 PEM。
 */
bool DrdTlsCredentials::reloadFromPem(const QString &certificatePem, const QString &keyPem, QString *error)
{
    Q_UNUSED(certificatePem);
    Q_UNUSED(keyPem);

    // 释放旧的证书和密钥
    if (m_certificate != nullptr)
    {
        freerdp_certificate_free(m_certificate);
        m_certificate = nullptr;
    }
    if (m_privateKey != nullptr)
    {
        freerdp_key_free(m_privateKey);
        m_privateKey = nullptr;
    }

    // 解析证书
    m_certificate = freerdp_certificate_new_from_pem(m_certificatePath.toUtf8().constData());
    if (m_certificate == nullptr)
    {
        if (error)
        {
            *error = "Failed to parse TLS certificate material";
        }
        return false;
    }

    // 解析私钥
    m_privateKey = freerdp_key_new_from_pem(m_privateKeyPath.toUtf8().constData());
    if (m_privateKey == nullptr)
    {
        freerdp_certificate_free(m_certificate);
        m_certificate = nullptr;
        if (error)
        {
            *error = "Failed to parse TLS private key material";
        }
        return false;
    }

    return true;
}