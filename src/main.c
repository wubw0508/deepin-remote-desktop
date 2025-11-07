#include "core/drd_application.h"

int
main(int argc, char **argv)
{
    g_autoptr(DrdApplication) app = drd_application_new();
    g_autoptr(GError) error = NULL;

    int status = drd_application_run(app, argc, argv, &error);
    if (status != 0 && error != NULL)
    {
        g_printerr("运行失败：%s\n", error->message);
    }

    return status;
}
