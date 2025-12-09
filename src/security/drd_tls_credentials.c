#include "security/drd_tls_credentials.h"

#include <gio/gio.h>

#include <freerdp/crypto/certificate.h>
#include <freerdp/crypto/privatekey.h>
#include <freerdp/settings.h>

struct _DrdTlsCredentials
{
    GObject parent_instance;

    gchar *certificate_path;
    gchar *private_key_path;
    gchar *certificate_pem;
    gchar *private_key_pem;
    rdpCertificate *certificate;
    rdpPrivateKey *private_key;
};

G_DEFINE_TYPE(DrdTlsCredentials, drd_tls_credentials, G_TYPE_OBJECT)

/*
 * 功能：释放 TLS 凭据持有的资源。
 * 逻辑：释放 FreeRDP 证书/私钥对象，清理路径与 PEM 字符串，最后交由父类 dispose。
 * 参数：object 基类指针，期望为 DrdTlsCredentials。
 * 外部接口：FreeRDP freerdp_certificate_free/freerdp_key_free；GLib g_clear_pointer/g_free；GObjectClass::dispose。
 */
static void
drd_tls_credentials_dispose(GObject *object)
{
    DrdTlsCredentials *self = DRD_TLS_CREDENTIALS(object);

    if (self->certificate != NULL)
    {
        freerdp_certificate_free(self->certificate);
        self->certificate = NULL;
    }

    if (self->private_key != NULL)
    {
        freerdp_key_free(self->private_key);
        self->private_key = NULL;
    }

    g_clear_pointer(&self->certificate_path, g_free);
    g_clear_pointer(&self->private_key_path, g_free);
    g_clear_pointer(&self->certificate_pem, g_free);
    g_clear_pointer(&self->private_key_pem, g_free);

    G_OBJECT_CLASS(drd_tls_credentials_parent_class)->dispose(object);
}

/*
 * 功能：绑定类级别的析构回调。
 * 逻辑：将自定义 dispose 安装到 GObjectClass。
 * 参数：klass 类结构。
 * 外部接口：GLib 类型系统。
 */
static void
drd_tls_credentials_class_init(DrdTlsCredentialsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = drd_tls_credentials_dispose;
}

/*
 * 功能：初始化 TLS 凭据实例字段。
 * 逻辑：将路径/PEM/对象指针置空。
 * 参数：self 凭据实例。
 * 外部接口：无额外外部库。
 */
static void
drd_tls_credentials_init(DrdTlsCredentials *self)
{
    self->certificate_path = NULL;
    self->private_key_path = NULL;
    self->certificate_pem = NULL;
    self->private_key_pem = NULL;
    self->certificate = NULL;
    self->private_key = NULL;
}

/*
 * 功能：用 PEM 文本解析并缓存 FreeRDP 证书/私钥。
 * 逻辑：解析 PEM 生成 rdpCertificate/rdpPrivateKey，替换旧缓存并复制 PEM 文本；解析失败写入错误。
 * 参数：self 凭据实例；certificate_pem 证书 PEM；private_key_pem 私钥 PEM；error 错误输出。
 * 外部接口：FreeRDP freerdp_certificate_new_from_pem/freerdp_key_new_from_pem；GLib g_set_error_literal/g_strdup/g_free。
 */
static gboolean
drd_tls_credentials_apply_pem(DrdTlsCredentials *self,
                              const gchar *certificate_pem,
                              const gchar *private_key_pem,
                              GError **error)
{
    g_return_val_if_fail(certificate_pem != NULL, FALSE);
    g_return_val_if_fail(private_key_pem != NULL, FALSE);

    rdpCertificate *certificate = freerdp_certificate_new_from_pem(certificate_pem);
    if (certificate == NULL)
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "Failed to parse TLS certificate material");
        return FALSE;
    }

    rdpPrivateKey *key = freerdp_key_new_from_pem(private_key_pem);
    if (key == NULL)
    {
        freerdp_certificate_free(certificate);
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "Failed to parse TLS private key material");
        return FALSE;
    }

    if (self->certificate != NULL)
    {
        freerdp_certificate_free(self->certificate);
    }
    if (self->private_key != NULL)
    {
        freerdp_key_free(self->private_key);
    }
    g_free(self->certificate_pem);
    g_free(self->private_key_pem);

    self->certificate = certificate;
    self->private_key = key;
    self->certificate_pem = g_strdup(certificate_pem);
    self->private_key_pem = g_strdup(private_key_pem);
    return TRUE;
}

/*
 * 功能：从文件加载 PEM 证书与私钥并应用。
 * 逻辑：读取两个文件内容后调用 apply_pem 解析，失败时前缀错误信息。
 * 参数：self 凭据实例；certificate_path/key_path 文件路径；error 错误输出。
 * 外部接口：GLib g_file_get_contents/g_prefix_error；内部 drd_tls_credentials_apply_pem。
 */
static gboolean
drd_tls_credentials_load(DrdTlsCredentials *self, const gchar *certificate_path, const gchar *private_key_path,
                         GError **error)
{
    g_autofree gchar *cert_data = NULL;
    if (!g_file_get_contents(certificate_path, &cert_data, NULL, error))
    {
        return FALSE;
    }

    g_autofree gchar *key_data = NULL;
    if (!g_file_get_contents(private_key_path, &key_data, NULL, error))
    {
        return FALSE;
    }

    if (!drd_tls_credentials_apply_pem(self, cert_data, key_data, error))
    {
        g_prefix_error(error, "Failed to load TLS material from files: ");
        return FALSE;
    }

    return TRUE;
}

/*
 * 功能：根据证书/私钥路径构造 TLS 凭据。
 * 逻辑：创建对象并保存路径，然后加载文件解析 PEM；失败则释放对象并返回 NULL。
 * 参数：certificate_path 证书路径；private_key_path 私钥路径；error 错误输出。
 * 外部接口：GLib g_object_new/g_strdup；内部 drd_tls_credentials_load。
 */
DrdTlsCredentials *
drd_tls_credentials_new(const gchar *certificate_path, const gchar *private_key_path, GError **error)
{
    g_return_val_if_fail(certificate_path != NULL, NULL);
    g_return_val_if_fail(private_key_path != NULL, NULL);

    DrdTlsCredentials *self = g_object_new(DRD_TYPE_TLS_CREDENTIALS, NULL);
    self->certificate_path = g_strdup(certificate_path);
    self->private_key_path = g_strdup(private_key_path);

    if (!drd_tls_credentials_load(self, certificate_path, private_key_path, error))
    {
        g_clear_object(&self);
        return NULL;
    }

    return self;
}

/*
 * 功能：创建空的 TLS 凭据对象（用于后续内存注入）。
 * 逻辑：简单调用 g_object_new。
 * 参数：无。
 * 外部接口：GLib g_object_new。
 */
DrdTlsCredentials *
drd_tls_credentials_new_empty(void)
{
    return g_object_new(DRD_TYPE_TLS_CREDENTIALS, NULL);
}

/*
 * 功能：获取证书路径。
 * 逻辑：类型检查后返回路径字符串。
 * 参数：self 凭据实例。
 * 外部接口：无额外外部库。
 */
const gchar *
drd_tls_credentials_get_certificate_path(DrdTlsCredentials *self)
{
    g_return_val_if_fail(DRD_IS_TLS_CREDENTIALS(self), NULL);
    return self->certificate_path;
}

/*
 * 功能：获取私钥路径。
 * 逻辑：类型检查后返回路径。
 * 参数：self 凭据实例。
 * 外部接口：无额外外部库。
 */
const gchar *
drd_tls_credentials_get_private_key_path(DrdTlsCredentials *self)
{
    g_return_val_if_fail(DRD_IS_TLS_CREDENTIALS(self), NULL);
    return self->private_key_path;
}

/*
 * 功能：获取 FreeRDP 证书对象。
 * 逻辑：类型检查后返回缓存的 rdpCertificate。
 * 参数：self 凭据实例。
 * 外部接口：无额外外部库。
 */
rdpCertificate *
drd_tls_credentials_get_certificate(DrdTlsCredentials *self)
{
    g_return_val_if_fail(DRD_IS_TLS_CREDENTIALS(self), NULL);
    return self->certificate;
}

/*
 * 功能：获取 FreeRDP 私钥对象。
 * 逻辑：类型检查后返回缓存的 rdpPrivateKey。
 * 参数：self 凭据实例。
 * 外部接口：无额外外部库。
 */
rdpPrivateKey *
drd_tls_credentials_get_private_key(DrdTlsCredentials *self)
{
    g_return_val_if_fail(DRD_IS_TLS_CREDENTIALS(self), NULL);
    return self->private_key;
}

/*
 * 功能：读取缓存的 PEM 文本。
 * 逻辑：若缓存存在则复制证书/私钥字符串到调用方，缺失时设置错误并清理已分配的输出。
 * 参数：self 凭据实例；certificate/key 输出字符串指针；error 错误输出。
 * 外部接口：GLib g_set_error_literal/g_strdup/g_free。
 */
gboolean
drd_tls_credentials_read_material(DrdTlsCredentials *self,
                                  gchar **certificate,
                                  gchar **key,
                                  GError **error)
{
    g_return_val_if_fail(DRD_IS_TLS_CREDENTIALS(self), FALSE);

    if (certificate != NULL)
    {
        if (self->certificate_pem == NULL)
        {
            g_set_error_literal(error,
                                G_IO_ERROR,
                                G_IO_ERROR_FAILED,
                                "TLS certificate material unavailable");
            return FALSE;
        }
        *certificate = g_strdup(self->certificate_pem);
    }

    if (key != NULL)
    {
        if (self->private_key_pem == NULL)
        {
            g_set_error_literal(error,
                                G_IO_ERROR,
                                G_IO_ERROR_FAILED,
                                "TLS private key material unavailable");
            if (certificate != NULL)
            {
                g_free(*certificate);
                *certificate = NULL;
            }
            return FALSE;
        }
        *key = g_strdup(self->private_key_pem);
    }

    return TRUE;
}

/*
 * 功能：从内存 PEM 重新加载 TLS 凭据。
 * 逻辑：委托 apply_pem 解析并替换缓存。
 * 参数：self 凭据实例；certificate_pem/key_pem PEM 文本；error 错误输出。
 * 外部接口：内部 drd_tls_credentials_apply_pem。
 */
gboolean
drd_tls_credentials_reload_from_pem(DrdTlsCredentials *self,
                                    const gchar *certificate_pem,
                                    const gchar *key_pem,
                                    GError **error)
{
    g_return_val_if_fail(DRD_IS_TLS_CREDENTIALS(self), FALSE);

    if (!drd_tls_credentials_apply_pem(self, certificate_pem, key_pem, error))
    {
        return FALSE;
    }

    return TRUE;
}

/*
 * 功能：将 TLS 凭据应用到 FreeRDP settings。
 * 逻辑：确保缓存的 PEM 存在后重新解析生成新的证书/私钥对象，并通过 freerdp_settings_set_pointer_len 注入 settings；任一步失败则写入错误并释放临时对象。
 * 参数：self 凭据实例；settings FreeRDP 设置；error 错误输出。
 * 外部接口：FreeRDP freerdp_certificate_new_from_pem/freerdp_key_new_from_pem/freerdp_settings_set_pointer_len；GLib g_set_error_literal。
 */
gboolean
drd_tls_credentials_apply(DrdTlsCredentials *self, rdpSettings *settings, GError **error)
{
    g_return_val_if_fail(DRD_IS_TLS_CREDENTIALS(self), FALSE);
    g_return_val_if_fail(settings != NULL, FALSE);

    if (self->certificate_pem == NULL || self->private_key_pem == NULL)
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "TLS material unavailable for settings");
        return FALSE;
    }

    rdpCertificate *certificate = freerdp_certificate_new_from_pem(self->certificate_pem);
    if (certificate == NULL)
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "Failed to parse TLS certificate material");
        return FALSE;
    }

    rdpPrivateKey *key = freerdp_key_new_from_pem(self->private_key_pem);
    if (key == NULL)
    {
        freerdp_certificate_free(certificate);
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "Failed to parse TLS private key material");
        return FALSE;
    }

    if (!freerdp_settings_set_pointer_len(settings,
                                          FreeRDP_RdpServerCertificate,
                                          certificate,
                                          1))
    {
        freerdp_certificate_free(certificate);
        freerdp_key_free(key);
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "Failed to assign server certificate to settings");
        return FALSE;
    }

    if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerRsaKey, key, 1))
    {
        freerdp_key_free(key);
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "Failed to assign private key to settings");
        return FALSE;
    }

    return TRUE;
}
