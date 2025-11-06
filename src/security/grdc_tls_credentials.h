#pragma once

#include <glib-object.h>

struct rdp_settings;
typedef struct rdp_settings rdpSettings;

typedef struct rdp_certificate rdpCertificate;
typedef struct rdp_private_key rdpPrivateKey;

G_BEGIN_DECLS

#define GRDC_TYPE_TLS_CREDENTIALS (grdc_tls_credentials_get_type())
G_DECLARE_FINAL_TYPE(GrdcTlsCredentials, grdc_tls_credentials, GRDC, TLS_CREDENTIALS, GObject)

GrdcTlsCredentials *grdc_tls_credentials_new(const gchar *certificate_path,
                                             const gchar *private_key_path,
                                             GError **error);

const gchar *grdc_tls_credentials_get_certificate_path(GrdcTlsCredentials *self);
const gchar *grdc_tls_credentials_get_private_key_path(GrdcTlsCredentials *self);

gboolean grdc_tls_credentials_apply(GrdcTlsCredentials *self, rdpSettings *settings, GError **error);

rdpCertificate *grdc_tls_credentials_get_certificate(GrdcTlsCredentials *self);
rdpPrivateKey *grdc_tls_credentials_get_private_key(GrdcTlsCredentials *self);

G_END_DECLS
