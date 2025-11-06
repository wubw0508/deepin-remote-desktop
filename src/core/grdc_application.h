#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define GRDC_TYPE_APPLICATION (grdc_application_get_type())
G_DECLARE_FINAL_TYPE(GrdcApplication, grdc_application, GRDC, APPLICATION, GObject)

GrdcApplication *grdc_application_new(void);
int grdc_application_run(GrdcApplication *self, int argc, char **argv, GError **error);

G_END_DECLS
