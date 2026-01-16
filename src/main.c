#include "core/drd_application.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSharedPointer>
#include <QString>

#include <freerdp/channels/channels.h>
#include <freerdp/primitives.h>
#include <winpr/ssl.h>
#include <winpr/wtsapi.h>

static bool drd_initialize_winpr(void) {
  /*
   * 功能：初始化 WinPR 相关子系统（SSL 与 WTS）。
   * 逻辑：初始化 WinPR SSL；获取 WTS API 表并注册；任一步失败打印错误并返回
   * FALSE。 参数：无。 外部接口：WinPR
   * winpr_InitializeSSL、FreeRDP_InitWtsApi/WTSRegisterWtsApiFunctionTable；Qt
   * qCritical 输出错误。
   */
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
  /*
   * 功能：程序入口，初始化 WinPR 并运行应用。
   * 逻辑：先初始化 WinPR；创建并运行
   * DrdApplication，失败时输出错误；返回运行状态码。 参数：argc/argv
   * 命令行参数。 外部接口：WinPR/FreeRDP 初始化；drd_application_run
   * 执行主逻辑；Qt qCritical 输出错误。
   */
  if (!drd_initialize_winpr()) {
    return 1;
  }
  primitives_get();
  QCoreApplication qt_app(argc, argv);
  (void)qt_app;

  QSharedPointer<DrdApplication> app(drd_application_new(),
                                     [](DrdApplication *value) {
                                       if (value != nullptr) {
                                         g_object_unref(value);
                                       }
                                     });
  if (app.isNull()) {
    qCritical().noquote() << QStringLiteral("create application failed");
    return 1;
  }

  const int status = drd_application_run(app.data(), argc, argv, nullptr);
  if (status != 0) {
    qCritical().noquote() << QStringLiteral("run error");
  }

  return status;
}
