#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QFile>
#include <QTextStream>

#include <freerdp/channels/channels.h>
#include <winpr/ssl.h>
#include <winpr/wtsapi.h>
#include <freerdp/primitives.h>

#include "core/drd_application.h"
#include "utils/drd_log.h"

/**
 * @brief 初始化WinPR相关子系统（SSL与WTS）
 * @return 初始化成功返回true，失败返回false
 */
static bool drd_initialize_winpr(void)
{
    /*
     * 功能：初始化 WinPR 相关子系统（SSL 与 WTS）。
     * 逻辑：初始化 WinPR SSL；获取 WTS API 表并注册；任一步失败打印错误并返回 FALSE。
     * 参数：无。
     * 外部接口：WinPR winpr_InitializeSSL、FreeRDP_InitWtsApi/WTSRegisterWtsApiFunctionTable；Qt qDebug 输出错误。
     */
    if (!winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT))
    {
        qCritical() << "WinPR SSL init failed";
        return false;
    }

    const WtsApiFunctionTable *table = FreeRDP_InitWtsApi();
    if (table == NULL || !WTSRegisterWtsApiFunctionTable(table))
    {
        qCritical() << "register WinPR WTS API failed";
        return false;
    }

    return true;
}

/**
 * @brief 程序入口，初始化WinPR并运行应用
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出状态码
 */
int main(int argc, char **argv)
{
    /*
     * 功能：程序入口，初始化 WinPR 并运行应用。
     * 逻辑：先初始化 WinPR；创建 Qt 应用实例；创建并运行 DrdApplication，失败时输出错误；返回运行状态码。
     * 参数：argc/argv 命令行参数。
     * 外部接口：WinPR/FreeRDP 初始化；Qt QCoreApplication；DrdApplication 执行主逻辑；Qt qCritical 输出错误。
     */
    
    // 初始化WinPR
    if (!drd_initialize_winpr())
    {
        return 1;
    }
    
    // 获取FreeRDP primitives
    primitives_get();
    
    // 初始化日志系统
    DrdLog::init();
    
    // 创建Qt应用实例
    QCoreApplication app(argc, argv);
    
    // 设置应用信息
    app.setApplicationName("deepin-remote-desktop");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Deepin");
    
    // 创建DrdApplication实例（使用 Qt 的 new 操作符）
    DrdApplication *drdApp = new DrdApplication(&app);
    if (drdApp == nullptr)
    {
        qCritical() << "Failed to create DrdApplication";
        return 1;
    }
    
    // 运行应用（使用 Qt 的 QString 错误处理）
    QString error;
    int status = drdApp->run(argc, argv, &error);
    
    // 处理错误
    if (status != 0 && !error.isEmpty())
    {
        qCritical() << "Run error:" << error;
    }
    
    // Qt 会自动清理子对象，无需手动 delete
    // drdApp 的父对象是 &app，会在 app 销毁时自动删除
    
    return status;
}