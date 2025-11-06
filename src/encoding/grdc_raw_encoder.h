#pragma once

#include <glib-object.h>

#include "utils/grdc_frame.h"
#include "utils/grdc_encoded_frame.h"

G_BEGIN_DECLS

#define GRDC_TYPE_RAW_ENCODER (grdc_raw_encoder_get_type())
G_DECLARE_FINAL_TYPE(GrdcRawEncoder, grdc_raw_encoder, GRDC, RAW_ENCODER, GObject)

GrdcRawEncoder *grdc_raw_encoder_new(void);

gboolean grdc_raw_encoder_configure(GrdcRawEncoder *self, guint width, guint height, GError **error);
void grdc_raw_encoder_reset(GrdcRawEncoder *self);

gboolean grdc_raw_encoder_encode(GrdcRawEncoder *self,
                                 GrdcFrame *input,
                                 GrdcEncodedFrame *output,
                                 GError **error);

G_END_DECLS
