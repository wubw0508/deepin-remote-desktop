#pragma once

#include <QString>
#include <QDebug>

/**
 * @brief Qt 版本的日志系统
 * 
 * 替代 GLib 的日志系统，使用 Qt 的日志框架
 */
namespace DrdLog {

/**
 * @brief 初始化日志系统
 */
void init();

/**
 * @brief 输出消息日志
 * @param format 日志格式字符串
 * @param ... 可变参数
 */
void message(const QString &format, ...);

/**
 * @brief 输出警告日志
 * @param format 日志格式字符串
 * @param ... 可变参数
 */
void warning(const QString &format, ...);

/**
 * @brief 输出错误日志
 * @param format 日志格式字符串
 * @param ... 可变参数
 */
void error(const QString &format, ...);

/**
 * @brief 输出调试日志
 * @param format 日志格式字符串
 * @param ... 可变参数
 */
void debug(const QString &format, ...);

/**
 * @brief 输出信息日志
 * @param format 日志格式字符串
 * @param ... 可变参数
 */
void info(const QString &format, ...);

} // namespace DrdLog

// 便捷宏定义
#define DRD_LOG_MESSAGE(...) DrdLog::message(__VA_ARGS__)
#define DRD_LOG_WARNING(...) DrdLog::warning(__VA_ARGS__)
#define DRD_LOG_ERROR(...) DrdLog::error(__VA_ARGS__)
#define DRD_LOG_DEBUG(...) DrdLog::debug(__VA_ARGS__)
#define DRD_LOG_INFO(...) DrdLog::info(__VA_ARGS__)