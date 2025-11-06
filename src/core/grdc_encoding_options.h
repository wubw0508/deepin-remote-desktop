#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
    GRDC_ENCODING_MODE_RAW,
    GRDC_ENCODING_MODE_RFX
} GrdcEncodingMode;

typedef enum
{
    GRDC_ENCODING_QUALITY_HIGH,
    GRDC_ENCODING_QUALITY_MEDIUM,
    GRDC_ENCODING_QUALITY_LOW
} GrdcEncodingQuality;

typedef struct
{
    guint width;
    guint height;
    GrdcEncodingMode mode;
    GrdcEncodingQuality quality;
    gboolean enable_frame_diff;
} GrdcEncodingOptions;

G_END_DECLS
