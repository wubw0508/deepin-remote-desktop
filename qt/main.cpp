#include "core/drd_application.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSharedPointer>
#include <QString>

// 禁用 pedantic 警告，因为 winpr3 头文件包含匿名结构体
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <freerdp/channels/channels.h>
#include <freerdp/primitives.h>
#include <winpr/ssl.h>
#include <winpr/wtsapi.h>
#pragma GCC diagnostic pop

static bool drd_initialize_winpr(void) {
    if (!winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT)) {
        qCritical().noquote() << QStringLiteral("WinPR SSL init failed");
        return false;
    }

    const WtsApiFunctionTable *table = FreeRDP_InitWtsApi();
    if (table == NULL || !WTSRegisterWtsApiFunctionTable(table)) {
        qCritical().noquote() << QStringLiteral("register WinPR WTS API failed");
        return false;
    }

    return true;
}

int main(int argc, char **argv) {
    if (!drd_initialize_winpr()) {
        return 1;
    }
    primitives_get();
    QCoreApplication qt_app(argc, argv);
    (void)qt_app;

    QSharedPointer<DrdQtApplication> app(new DrdQtApplication(),
                                       [](DrdQtApplication *value) {
                                         if (value != nullptr) {
                                           delete value;
                                         }
                                       });
    if (app.isNull()) {
        qCritical().noquote() << QStringLiteral("create application failed");
        return 1;
    }

    QStringList arguments;
    for (int i = 0; i < argc; ++i) {
        arguments.append(QString::fromUtf8(argv[i]));
    }

    QString error_message;
    const int status = app->drd_application_run(arguments, &error_message);
    if (status != 0) {
        qCritical().noquote() << QStringLiteral("run error: %1").arg(error_message);
    }

    return status;
}