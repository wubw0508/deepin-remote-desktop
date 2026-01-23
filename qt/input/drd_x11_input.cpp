#include "input/drd_x11_input.h"

#include <QDebug>

#include <freerdp/freerdp.h>
#include <freerdp/input.h>
#include <winpr/wtypes.h>
#include <winpr/input.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

/**
 * @brief 处理键盘事件
 * @param input FreeRDP 输入接口
 * @param flags 按键标志
 * @param code 扫描码
 * @return 成功返回 TRUE
 */
BOOL DrdX11Input::drd_rdp_peer_keyboard_event_x11(rdpInput *input, UINT16 flags, UINT8 code)
{
    if (input == nullptr || input->context == nullptr)
    {
        return TRUE;
    }

    // 获取 X11 显示
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        qWarning() << "Failed to open X11 display for keyboard event";
        return TRUE;
    }

    DWORD vkcode = 0;
    DWORD keycode = 0;
    DWORD scancode = code;
    BOOL extended = FALSE;

    if (flags & KBD_FLAGS_EXTENDED)
        extended = TRUE;

    if (extended)
        scancode |= KBDEXT;

    vkcode = GetVirtualKeyCodeFromVirtualScanCode(scancode, WINPR_KBD_TYPE_IBM_ENHANCED);

    if (extended)
        vkcode |= KBDEXT;

    keycode = GetKeycodeFromVirtualKeyCode(vkcode, WINPR_KEYCODE_TYPE_XKB);

    qDebug() << "Keyboard event conversion: code=" << code << "(0x" << QString::number(code, 16) 
             << "), flags=0x" << QString::number(flags, 16) << ", extended=" << extended
             << ", scancode=" << scancode << "(0x" << QString::number(scancode, 16)
             << "), vkcode=" << vkcode << "(0x" << QString::number(vkcode, 16)
             << "), keycode=" << keycode << "(0x" << QString::number(keycode, 16)
             << "), event_type=" << ((flags & KBD_FLAGS_RELEASE) ? "release" : "press");

    if (keycode != 0)
    {
        XLockDisplay(display);
        XTestGrabControl(display, True);

        if (flags & KBD_FLAGS_RELEASE)
            XTestFakeKeyEvent(display, keycode, False, CurrentTime);
        else
            XTestFakeKeyEvent(display, keycode, True, CurrentTime);

        XTestGrabControl(display, False);
        XFlush(display);
        XUnlockDisplay(display);
    }

    XCloseDisplay(display);
    return TRUE;
}

/**
 * @brief 处理 Unicode 键盘事件
 * @param input FreeRDP 输入接口
 * @param flags 按键标志
 * @param code Unicode 码点
 * @return 成功返回 TRUE
 */
BOOL DrdX11Input::drd_rdp_peer_unicode_event_x11(rdpInput *input, UINT16 flags, UINT16 code)
{
    if (input == nullptr || input->context == nullptr)
    {
        return TRUE;
    }

    // Unicode 输入暂不支持
    qDebug() << "Unicode keyboard event received: code=" << code << "(0x" << QString::number(code, 16)
             << "), flags=0x" << QString::number(flags, 16);
    
    return TRUE;
}

/**
 * @brief 处理鼠标事件
 * @param input FreeRDP 输入接口
 * @param flags 鼠标标志
 * @param x X 坐标
 * @param y Y 坐标
 * @return 成功返回 TRUE
 */
BOOL DrdX11Input::drd_rdp_peer_pointer_event_x11(rdpInput *input, UINT16 flags, UINT16 x, UINT16 y)
{
    if (input == nullptr || input->context == nullptr)
    {
        return TRUE;
    }

    // 获取 X11 显示
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        qWarning() << "Failed to open X11 display for pointer event";
        return TRUE;
    }

    unsigned int button = 0;
    BOOL down = FALSE;

    XLockDisplay(display);
    XTestGrabControl(display, True);

    if (flags & PTR_FLAGS_WHEEL)
    {
        BOOL negative = FALSE;

        if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
            negative = TRUE;

        button = (negative) ? 5 : 4;
        XTestFakeButtonEvent(display, button, True, (unsigned long)CurrentTime);
        XTestFakeButtonEvent(display, button, False, (unsigned long)CurrentTime);
    }
    else if (flags & PTR_FLAGS_HWHEEL)
    {
        BOOL negative = FALSE;

        if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
            negative = TRUE;

        button = (negative) ? 7 : 6;
        XTestFakeButtonEvent(display, button, True, (unsigned long)CurrentTime);
        XTestFakeButtonEvent(display, button, False, (unsigned long)CurrentTime);
    }
    else
    {
        if (flags & PTR_FLAGS_MOVE)
            XTestFakeMotionEvent(display, 0, x, y, CurrentTime);

        if (flags & PTR_FLAGS_BUTTON1)
            button = 1;
        else if (flags & PTR_FLAGS_BUTTON2)
            button = 3;
        else if (flags & PTR_FLAGS_BUTTON3)
            button = 2;

        if (flags & PTR_FLAGS_DOWN)
            down = TRUE;

        if (button)
            XTestFakeButtonEvent(display, button, down, CurrentTime);
    }

    XTestGrabControl(display, False);
    XFlush(display);
    XUnlockDisplay(display);
    XCloseDisplay(display);

    return TRUE;
}

/**
 * @brief 处理扩展鼠标事件
 * @param input FreeRDP 输入接口
 * @param flags 鼠标标志
 * @param x X 坐标
 * @param y Y 坐标
 * @return 成功返回 TRUE
 */
BOOL DrdX11Input::drd_rdp_peer_extended_pointer_event(rdpInput *input, UINT16 flags, UINT16 x, UINT16 y)
{
    if (input == nullptr || input->context == nullptr)
    {
        return TRUE;
    }

    // 获取 X11 显示
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        qWarning() << "Failed to open X11 display for extended pointer event";
        return TRUE;
    }

    XLockDisplay(display);
    XTestGrabControl(display, True);
    XTestFakeMotionEvent(display, 0, x, y, CurrentTime);

    UINT button = 0;
    BOOL down = FALSE;

    if (flags & PTR_XFLAGS_BUTTON1)
        button = 8;
    else if (flags & PTR_XFLAGS_BUTTON2)
        button = 9;

    if (flags & PTR_XFLAGS_DOWN)
        down = TRUE;

    if (button)
        XTestFakeButtonEvent(display, button, down, CurrentTime);

    XTestGrabControl(display, False);
    XFlush(display);
    XUnlockDisplay(display);
    XCloseDisplay(display);

    return TRUE;
}