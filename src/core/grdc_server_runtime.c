#include "core/grdc_server_runtime.h"

#include <gio/gio.h>

struct _GrdcServerRuntime
{
    GObject parent_instance;

    GrdcCaptureManager *capture;
    GrdcEncodingManager *encoder;
    GrdcInputDispatcher *input;
    GrdcTlsCredentials *tls;
    GrdcEncodingOptions encoding_options;
    gboolean has_encoding_options;
};

G_DEFINE_TYPE(GrdcServerRuntime, grdc_server_runtime, G_TYPE_OBJECT)

static void
grdc_server_runtime_dispose(GObject *object)
{
    GrdcServerRuntime *self = GRDC_SERVER_RUNTIME(object);
    grdc_server_runtime_stop(self);
    g_clear_object(&self->capture);
    g_clear_object(&self->encoder);
    g_clear_object(&self->input);
    g_clear_object(&self->tls);

    G_OBJECT_CLASS(grdc_server_runtime_parent_class)->dispose(object);
}

static void
grdc_server_runtime_class_init(GrdcServerRuntimeClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = grdc_server_runtime_dispose;
}

static void
grdc_server_runtime_init(GrdcServerRuntime *self)
{
    self->capture = grdc_capture_manager_new();
    self->encoder = grdc_encoding_manager_new();
    self->input = grdc_input_dispatcher_new();
    self->tls = NULL;
    self->has_encoding_options = FALSE;
}

GrdcServerRuntime *
grdc_server_runtime_new(void)
{
    return g_object_new(GRDC_TYPE_SERVER_RUNTIME, NULL);
}

GrdcCaptureManager *
grdc_server_runtime_get_capture(GrdcServerRuntime *self)
{
    g_return_val_if_fail(GRDC_IS_SERVER_RUNTIME(self), NULL);
    return self->capture;
}

GrdcEncodingManager *
grdc_server_runtime_get_encoder(GrdcServerRuntime *self)
{
    g_return_val_if_fail(GRDC_IS_SERVER_RUNTIME(self), NULL);
    return self->encoder;
}

GrdcInputDispatcher *
grdc_server_runtime_get_input(GrdcServerRuntime *self)
{
    g_return_val_if_fail(GRDC_IS_SERVER_RUNTIME(self), NULL);
    return self->input;
}

gboolean
grdc_server_runtime_prepare_stream(GrdcServerRuntime *self,
                                   const GrdcEncodingOptions *encoding_options,
                                   GError **error)
{
    g_return_val_if_fail(GRDC_IS_SERVER_RUNTIME(self), FALSE);
    g_return_val_if_fail(encoding_options != NULL, FALSE);

    self->encoding_options = *encoding_options;
    self->has_encoding_options = TRUE;

    if (!grdc_encoding_manager_prepare(self->encoder, encoding_options, error))
    {
        return FALSE;
    }

    if (!grdc_input_dispatcher_start(self->input,
                                     encoding_options->width,
                                     encoding_options->height,
                                     error))
    {
        grdc_encoding_manager_reset(self->encoder);
        return FALSE;
    }

    if (!grdc_capture_manager_start(self->capture,
                                    encoding_options->width,
                                    encoding_options->height,
                                    error))
    {
        grdc_input_dispatcher_stop(self->input);
        grdc_encoding_manager_reset(self->encoder);
        return FALSE;
    }

    g_message("Server runtime prepared stream with geometry %ux%u",
              encoding_options->width,
              encoding_options->height);
    return TRUE;
}

void
grdc_server_runtime_stop(GrdcServerRuntime *self)
{
    g_return_if_fail(GRDC_IS_SERVER_RUNTIME(self));

    grdc_capture_manager_stop(self->capture);
    grdc_encoding_manager_reset(self->encoder);
    grdc_input_dispatcher_flush(self->input);
    grdc_input_dispatcher_stop(self->input);
    self->has_encoding_options = FALSE;
}

gboolean
grdc_server_runtime_pull_encoded_frame(GrdcServerRuntime *self,
                                        gint64 timeout_us,
                                        gsize max_payload,
                                        GrdcEncodedFrame **out_frame,
                                        GError **error)
{
    g_return_val_if_fail(GRDC_IS_SERVER_RUNTIME(self), FALSE);
    g_return_val_if_fail(out_frame != NULL, FALSE);

    GrdcFrame *frame = NULL;
    if (!grdc_capture_manager_wait_frame(self->capture, timeout_us, &frame, error))
    {
        return FALSE;
    }

    gboolean ok = grdc_encoding_manager_encode(self->encoder, frame, max_payload, out_frame, error);
    g_object_unref(frame);

    if (!ok)
    {
        return FALSE;
    }

    return TRUE;
}

GrdcFrameCodec
grdc_server_runtime_get_codec(GrdcServerRuntime *self)
{
    g_return_val_if_fail(GRDC_IS_SERVER_RUNTIME(self), GRDC_FRAME_CODEC_RAW);
    return grdc_encoding_manager_get_codec(self->encoder);
}

gboolean
grdc_server_runtime_get_encoding_options(GrdcServerRuntime *self,
                                         GrdcEncodingOptions *out_options)
{
    g_return_val_if_fail(GRDC_IS_SERVER_RUNTIME(self), FALSE);
    g_return_val_if_fail(out_options != NULL, FALSE);

    if (!self->has_encoding_options)
    {
        return FALSE;
    }

    *out_options = self->encoding_options;
    return TRUE;
}

void
grdc_server_runtime_set_tls_credentials(GrdcServerRuntime *self, GrdcTlsCredentials *credentials)
{
    g_return_if_fail(GRDC_IS_SERVER_RUNTIME(self));
    if (credentials != NULL)
    {
        g_object_ref(credentials);
    }
    g_clear_object(&self->tls);
    self->tls = credentials;
}

GrdcTlsCredentials *
grdc_server_runtime_get_tls_credentials(GrdcServerRuntime *self)
{
    g_return_val_if_fail(GRDC_IS_SERVER_RUNTIME(self), NULL);
    return self->tls;
}
