#pragma once

#include <QString>

/**
 * @brief 编码模式枚举
 */
enum class DrdEncodingMode
{
    Raw = 0,
    Rfx,
    H264,
    Auto
};

/**
 * @brief 运行模式枚举
 */
enum class DrdRuntimeMode
{
    User = 0,
    System,
    Handover
};

/**
 * @brief 编码选项结构体
 */
struct DrdEncodingOptions
{
    unsigned int width = 0;
    unsigned int height = 0;
    DrdEncodingMode mode = DrdEncodingMode::Auto;
    bool enableFrameDiff = false;
    unsigned int h264Bitrate = 5000000;
    unsigned int h264Framerate = 60;
    unsigned int h264Qp = 15;
    bool h264HwAccel = false;
    bool h264VmSupport = false;
    double gfxLargeChangeThreshold = 0.05;
    unsigned int gfxProgressiveRefreshInterval = 6;
    unsigned int gfxProgressiveRefreshTimeoutMs = 100;
};

/**
 * @brief 将编码模式转换为字符串
 * @param mode 编码模式
 * @return 模式字符串
 */
inline QString drdEncodingModeToString(DrdEncodingMode mode)
{
    switch (mode)
    {
        case DrdEncodingMode::Raw:
            return "raw";
        case DrdEncodingMode::Rfx:
            return "rfx";
        case DrdEncodingMode::H264:
            return "h264";
        case DrdEncodingMode::Auto:
            return "auto";
        default:
            return "unknown";
    }
}

/**
 * @brief 将字符串转换为编码模式
 * @param str 模式字符串
 * @return 编码模式
 */
inline DrdEncodingMode drdStringToEncodingMode(const QString &str)
{
    if (str == "raw")
        return DrdEncodingMode::Raw;
    if (str == "rfx")
        return DrdEncodingMode::Rfx;
    if (str == "h264")
        return DrdEncodingMode::H264;
    if (str == "auto")
        return DrdEncodingMode::Auto;
    return DrdEncodingMode::Auto;
}

/**
 * @brief 将运行模式转换为字符串
 * @param mode 运行模式
 * @return 模式字符串
 */
inline QString drdRuntimeModeToString(DrdRuntimeMode mode)
{
    switch (mode)
    {
        case DrdRuntimeMode::System:
            return "system";
        case DrdRuntimeMode::Handover:
            return "handover";
        case DrdRuntimeMode::User:
        default:
            return "user";
    }
}

/**
 * @brief 将字符串转换为运行模式
 * @param str 模式字符串
 * @return 运行模式
 */
inline DrdRuntimeMode drdStringToRuntimeMode(const QString &str)
{
    if (str == "system")
        return DrdRuntimeMode::System;
    if (str == "handover")
        return DrdRuntimeMode::Handover;
    return DrdRuntimeMode::User;
}