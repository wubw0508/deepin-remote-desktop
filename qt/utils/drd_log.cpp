#include "drd_log.h"
#include <cstdarg>
#include <cstdio>

namespace DrdLog {

/**
 * @brief 初始化日志系统
 * 
 * 功能：初始化 Qt 日志系统，设置日志格式。
 * 逻辑：配置 Qt 消息处理器，设置日志输出格式。
 * 参数：无。
 * 外部接口：Qt qInstallMessageHandler。
 */
void init()
{
    // Qt 日志系统默认已经初始化
    // 可以在这里设置自定义的日志格式
    qSetMessagePattern("[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] %{message}");
}

/**
 * @brief 输出消息日志
 * 
 * 功能：输出 INFO 级别的日志消息。
 * 逻辑：使用 Qt 的 qInfo 输出格式化消息。
 * 参数：format 日志格式字符串，... 可变参数。
 * 外部接口：Qt qInfo。
 */
void message(const QString &format, ...)
{
    va_list args;
    va_start(args, format);
    
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format.toUtf8().constData(), args);
    
    qInfo().noquote() << QString::fromUtf8(buffer);
    
    va_end(args);
}

/**
 * @brief 输出警告日志
 * 
 * 功能：输出 WARNING 级别的日志消息。
 * 逻辑：使用 Qt 的 qWarning 输出格式化消息。
 * 参数：format 日志格式字符串，... 可变参数。
 * 外部接口：Qt qWarning。
 */
void warning(const QString &format, ...)
{
    va_list args;
    va_start(args, format);
    
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format.toUtf8().constData(), args);
    
    qWarning().noquote() << QString::fromUtf8(buffer);
    
    va_end(args);
}

/**
 * @brief 输出错误日志
 * 
 * 功能：输出 ERROR 级别的日志消息。
 * 逻辑：使用 Qt 的 qCritical 输出格式化消息。
 * 参数：format 日志格式字符串，... 可变参数。
 * 外部接口：Qt qCritical。
 */
void error(const QString &format, ...)
{
    va_list args;
    va_start(args, format);
    
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format.toUtf8().constData(), args);
    
    qCritical().noquote() << QString::fromUtf8(buffer);
    
    va_end(args);
}

/**
 * @brief 输出调试日志
 * 
 * 功能：输出 DEBUG 级别的日志消息。
 * 逻辑：使用 Qt 的 qDebug 输出格式化消息。
 * 参数：format 日志格式字符串，... 可变参数。
 * 外部接口：Qt qDebug。
 */
void debug(const QString &format, ...)
{
    va_list args;
    va_start(args, format);
    
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format.toUtf8().constData(), args);
    
    qDebug().noquote() << QString::fromUtf8(buffer);
    
    va_end(args);
}

/**
 * @brief 输出信息日志
 * 
 * 功能：输出 INFO 级别的日志消息。
 * 逻辑：使用 Qt 的 qInfo 输出格式化消息。
 * 参数：format 日志格式字符串，... 可变参数。
 * 外部接口：Qt qInfo。
 */
void info(const QString &format, ...)
{
    va_list args;
    va_start(args, format);
    
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format.toUtf8().constData(), args);
    
    qInfo().noquote() << QString::fromUtf8(buffer);
    
    va_end(args);
}

} // namespace DrdLog