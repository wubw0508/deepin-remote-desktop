#pragma once

#include <QObject>
#include <freerdp/freerdp.h>

/**
 * @brief X11 输入事件处理函数
 * 
 * 用于处理来自 RDP 客户端的键盘和鼠标输入事件
 */
class DrdX11Input : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 处理键盘事件
     * @param input FreeRDP 输入接口
     * @param flags 按键标志
     * @param code 扫描码
     * @return 成功返回 TRUE
     */
    static BOOL drd_rdp_peer_keyboard_event_x11(rdpInput *input, UINT16 flags, UINT8 code);

    /**
     * @brief 处理 Unicode 键盘事件
     * @param input FreeRDP 输入接口
     * @param flags 按键标志
     * @param code Unicode 码点
     * @return 成功返回 TRUE
     */
    static BOOL drd_rdp_peer_unicode_event_x11(rdpInput *input, UINT16 flags, UINT16 code);

    /**
     * @brief 处理鼠标事件
     * @param input FreeRDP 输入接口
     * @param flags 鼠标标志
     * @param x X 坐标
     * @param y Y 坐标
     * @return 成功返回 TRUE
     */
    static BOOL drd_rdp_peer_pointer_event_x11(rdpInput *input, UINT16 flags, UINT16 x, UINT16 y);

    /**
     * @brief 处理扩展鼠标事件
     * @param input FreeRDP 输入接口
     * @param flags 鼠标标志
     * @param x X 坐标
     * @param y Y 坐标
     * @return 成功返回 TRUE
     */
    static BOOL drd_rdp_peer_extended_pointer_event(rdpInput *input, UINT16 flags, UINT16 x, UINT16 y);
};