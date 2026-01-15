#include "input/drd_x11_input.h"

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <freerdp/input.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/scancode.h>

#include <gio/gio.h>
#include <math.h>
#include <string.h>

#include "utils/drd_log.h"

#define DRD_X11_KEYCODE_CACHE_SIZE 512
#define DRD_X11_KEYCODE_CACHE_INVALID ((guint16) 0xFFFF)

struct _DrdX11Input
{
    GObject parent_instance;

    GMutex lock;
    Display *display;
    gint screen;
    guint desktop_width;
    guint desktop_height;
    guint stream_width;
    guint stream_height;
    gboolean running;
    guint32 keyboard_layout;
    guint16 keycode_cache[DRD_X11_KEYCODE_CACHE_SIZE];
    gdouble stream_to_desktop_scale_x;
    gdouble stream_to_desktop_scale_y;
};

G_DEFINE_TYPE(DrdX11Input, drd_x11_input, G_TYPE_OBJECT)

static gboolean drd_x11_input_open_display(DrdX11Input * self, GError * *error);
static void drd_x11_input_close_display(DrdX11Input * self);

static KeyCode drd_x11_input_lookup_modifier_keycode(DrdX11Input *self,
                                                     guint8 scancode,
                                                     gboolean extended);

static void drd_x11_input_reset_keycode_cache(DrdX11Input *self);

static guint16 drd_x11_input_resolve_keycode(DrdX11Input *self,
                                             guint8 base_scancode,
                                             gboolean extended,
                                             gboolean *out_cache_miss);

static void drd_x11_input_refresh_pointer_scale(DrdX11Input *self);

static KeySym drd_x11_input_keysym_from_codepoint(gunichar codepoint);

/*
 * 功能：释放 X11 输入对象持有的资源。
 * 逻辑：停止输入后端、清理互斥锁并交由父类 dispose。
 * 参数：object 基类指针，期望为 DrdX11Input。
 * 外部接口：drd_x11_input_stop 关闭 X11 连接；GLib g_mutex_clear。
 */
static void
drd_x11_input_dispose(GObject *object)
{
    DrdX11Input *self = DRD_X11_INPUT(object);
    drd_x11_input_stop(self);
    g_mutex_clear(&self->lock);

    G_OBJECT_CLASS(drd_x11_input_parent_class)->dispose(object);
}

/*
 * 功能：绑定类级别析构回调。
 * 逻辑：将自定义 dispose 挂载到 GObjectClass。
 * 参数：klass 类结构。
 * 外部接口：GLib 类型系统。
 */
static void
drd_x11_input_class_init(DrdX11InputClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = drd_x11_input_dispose;
}

/*
 * 功能：初始化输入后端的默认字段。
 * 逻辑：初始化互斥锁，清零显示/尺寸/键盘布局，重置 keycode 缓存与指针缩放。
 * 参数：self 输入实例。
 * 外部接口：GLib g_mutex_init。
 */
static void
drd_x11_input_init(DrdX11Input *self)
{
    g_mutex_init(&self->lock);
    self->display = NULL;
    self->screen = 0;
    self->desktop_width = 0;
    self->desktop_height = 0;
    self->stream_width = 0;
    self->stream_height = 0;
    self->running = FALSE;
    self->keyboard_layout = 0;
    drd_x11_input_reset_keycode_cache(self);
    self->stream_to_desktop_scale_x = 1.0;
    self->stream_to_desktop_scale_y = 1.0;
}

/*
 * 功能：创建 X11 输入后端对象。
 * 逻辑：调用 g_object_new 分配实例。
 * 参数：无。
 * 外部接口：GLib g_object_new。
 */
DrdX11Input *
drd_x11_input_new(void)
{
    return g_object_new(DRD_TYPE_X11_INPUT, NULL);
}

/*
 * 功能：打开 X11 Display 并初始化屏幕与键盘布局信息。
 * 逻辑：若已打开则直接返回；调用 XOpenDisplay 获取连接并校验 XTest 扩展；记录屏幕尺寸与编码流尺寸；通过 FreeRDP keyboard API 初始化布局；最后计算指针缩放。
 * 参数：self 输入实例；error 错误输出。
 * 外部接口：X11 XOpenDisplay/DefaultScreen/DisplayWidth/DisplayHeight、XTestQueryExtension 校验扩展；FreeRDP freerdp_keyboard_init；GLib g_set_error_literal。
 */
static gboolean
drd_x11_input_open_display(DrdX11Input *self, GError **error)
{
    if (self->display != NULL)
    {
        return TRUE;
    }

    Display *display = XOpenDisplay(NULL);
    if (display == NULL)
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "X11 input injector failed to open default display");
        return FALSE;
    }

    int event_base = 0;
    int error_base = 0;
    int major = 0;
    int minor = 0;
    if (!XTestQueryExtension(display, &event_base, &error_base, &major, &minor))
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_NOT_SUPPORTED,
                            "X11 XTest extension not available");
        XCloseDisplay(display);
        return FALSE;
    }

    self->display = display;
    self->screen = DefaultScreen(display);
    self->desktop_width = (guint) DisplayWidth(display, self->screen);
    self->desktop_height = (guint) DisplayHeight(display, self->screen);
    if (self->desktop_width == 0)
    {
        self->desktop_width = 1920;
    }
    if (self->desktop_height == 0)
    {
        self->desktop_height = 1080;
    }
    if (self->stream_width == 0)
    {
        self->stream_width = self->desktop_width;
    }
    if (self->stream_height == 0)
    {
        self->stream_height = self->desktop_height;
    }

    self->keyboard_layout = freerdp_keyboard_init(0);
    if (self->keyboard_layout == 0)
    {
        self->keyboard_layout = freerdp_keyboard_init(KBD_US);
    }

    drd_x11_input_refresh_pointer_scale(self);
    return TRUE;
}

/*
 * 功能：关闭 X11 Display 连接。
 * 逻辑：若 display 有效则调用 XCloseDisplay 并清空指针。
 * 参数：self 输入实例。
 * 外部接口：XCloseDisplay。
 */
static void
drd_x11_input_close_display(DrdX11Input *self)
{
    if (self->display != NULL)
    {
        XCloseDisplay(self->display);
        self->display = NULL;
    }
}

/*
 * 功能：启动输入注入器。
 * 逻辑：持锁检查运行状态，必要时打开 X11 Display 并置 running 为 TRUE。
 * 参数：self 输入实例；error 错误输出。
 * 外部接口：drd_x11_input_open_display（X11/FreeRDP）；GLib g_mutex_lock/unlock。
 */
gboolean
drd_x11_input_start(DrdX11Input *self, GError **error)
{
    g_return_val_if_fail(DRD_IS_X11_INPUT(self), FALSE);

    g_mutex_lock(&self->lock);
    if (self->running)
    {
        g_mutex_unlock(&self->lock);
        return TRUE;
    }

    gboolean ok = drd_x11_input_open_display(self, error);
    if (ok)
    {
        self->running = TRUE;
    }
    g_mutex_unlock(&self->lock);
    return ok;
}

/*
 * 功能：停止输入注入器。
 * 逻辑：持锁检查运行标志；关闭 X11 Display、重置缓存与缩放后清除 running。
 * 参数：self 输入实例。
 * 外部接口：XCloseDisplay（通过 drd_x11_input_close_display）；GLib g_mutex_lock/unlock。
 */
void
drd_x11_input_stop(DrdX11Input *self)
{
    g_return_if_fail(DRD_IS_X11_INPUT(self));

    g_mutex_lock(&self->lock);
    if (!self->running)
    {
        g_mutex_unlock(&self->lock);
        return;
    }

    drd_x11_input_close_display(self);
    self->running = FALSE;
    drd_x11_input_reset_keycode_cache(self);
    self->stream_to_desktop_scale_x = 1.0;
    self->stream_to_desktop_scale_y = 1.0;
    g_mutex_unlock(&self->lock);
}

/*
 * 功能：更新编码流尺寸用于指针坐标映射。
 * 逻辑：持锁写入宽高并刷新缩放因子。
 * 参数：self 输入实例；width/height 新流尺寸。
 * 外部接口：内部 drd_x11_input_refresh_pointer_scale；GLib g_mutex。
 */
void
drd_x11_input_update_desktop_size(DrdX11Input *self, guint width, guint height)
{
    g_return_if_fail(DRD_IS_X11_INPUT(self));

    g_mutex_lock(&self->lock);
    if (width > 0)
    {
        self->stream_width = width;
    }
    if (height > 0)
    {
        self->stream_height = height;
    }
    drd_x11_input_refresh_pointer_scale(self);
    g_mutex_unlock(&self->lock);
}

/*
 * 功能：校验输入注入器是否处于运行状态。
 * 逻辑：检查 running 与 display；未运行时设置错误。
 * 参数：self 输入实例；error 错误输出。
 * 外部接口：GLib g_set_error_literal。
 */
static gboolean
drd_x11_input_check_running(DrdX11Input *self, GError **error)
{
    if (!self->running || self->display == NULL)
    {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "X11 input injector is not running");
        return FALSE;
    }
    return TRUE;
}

/*
 * 功能：将 RDP 键盘事件转换为 X11 事件注入。
 * 逻辑：持锁校验运行态；解析扩展/释放标志并解析 RDP 扫描码到 X11 keycode（缓存 + FreeRDP 映射 + 特殊键查找）；注入 XTest FakeKeyEvent 并刷新。
 * 参数：self 输入实例；flags RDP 键盘标志；scancode RDP 基础扫描码；error 错误输出。
 * 外部接口：FreeRDP MAKE_RDP_SCANCODE/RDP_SCANCODE_CODE/freerdp_keyboard_get_x11_keycode_from_scancode；XTestFakeKeyEvent/XFlush；日志 DRD_LOG_DEBUG。
 */
gboolean
drd_x11_input_inject_keyboard(DrdX11Input *self, guint16 flags, guint8 scancode, GError **error)
{
    g_return_val_if_fail(DRD_IS_X11_INPUT(self), FALSE);

    g_mutex_lock(&self->lock);
    if (!drd_x11_input_check_running(self, error))
    {
        DRD_LOG_WARNING("Keyboard injection failed - input injector not running, flags: 0x%04X, scancode: 0x%02X",
                      flags, scancode);
        g_mutex_unlock(&self->lock);
        return FALSE;
    }

    const gboolean release = (flags & KBD_FLAGS_RELEASE) != 0;
    const gboolean extended = (flags & (KBD_FLAGS_EXTENDED | KBD_FLAGS_EXTENDED1)) != 0;
    const UINT32 rdp_scancode = MAKE_RDP_SCANCODE(scancode, extended);
    const guint8 base_scancode = (guint8) RDP_SCANCODE_CODE(rdp_scancode);
    gboolean cache_miss = FALSE;
    guint16 x11_keycode =
            drd_x11_input_resolve_keycode(self, base_scancode, extended, &cache_miss);

    if (x11_keycode == 0)
    {
        if (cache_miss)
        {
            DRD_LOG_DEBUG("Could not translate RDP scancode 0x%02X (extended=%s)",
                          base_scancode,
                          extended ? "true" : "false");
        }
        g_mutex_unlock(&self->lock);
        return TRUE;
    }

    XTestFakeKeyEvent(self->display,
                      (unsigned int) x11_keycode,
                      release ? False : True,
                      CurrentTime);
    XFlush(self->display);

    DRD_LOG_DEBUG("Keyboard injection - flags: 0x%04X, scancode: 0x%02X, release: %s, extended: %s, "
                  "rdp_scancode: 0x%08X, base_scancode: 0x%02X, x11_keycode: %u, cache_miss: %s, action: %s",
                  flags, scancode, release ? "true" : "false", extended ? "true" : "false",
                  rdp_scancode, base_scancode, x11_keycode, cache_miss ? "true" : "false",
                  release ? "release" : "press");

    g_mutex_unlock(&self->lock);
    return TRUE;
}

/*
 * 功能：注入 Unicode 键盘事件。
 * 逻辑：持锁检查运行态；将 Unicode 转为 KeySym 再转 keycode，利用 XTest 注入按下/释放；无法映射时记录 debug 并返回成功以避免断流。
 * 参数：self 输入实例；flags RDP 键盘标志；codepoint Unicode 码点；error 错误输出。
 * 外部接口：XKeysymToKeycode、XTestFakeKeyEvent/XFlush；日志 DRD_LOG_DEBUG；内部 drd_x11_input_keysym_from_codepoint。
 */
gboolean
drd_x11_input_inject_unicode(DrdX11Input *self, guint16 flags, guint16 codepoint, GError **error)
{
    g_return_val_if_fail(DRD_IS_X11_INPUT(self), FALSE);

    g_mutex_lock(&self->lock);
    if (!drd_x11_input_check_running(self, error))
    {
        g_mutex_unlock(&self->lock);
        return FALSE;
    }

    const gboolean release = (flags & KBD_FLAGS_RELEASE) != 0;
    const gunichar ch = (gunichar) codepoint;
    KeySym keysym = drd_x11_input_keysym_from_codepoint(ch);
    if (keysym == NoSymbol)
    {
        DRD_LOG_DEBUG("Unsupported Unicode input U+%04X", codepoint);
        g_mutex_unlock(&self->lock);
        return TRUE;
    }

    KeyCode keycode = XKeysymToKeycode(self->display, keysym);
    if (keycode == 0)
    {
        DRD_LOG_DEBUG("No X11 keycode mapped for Unicode U+%04X", codepoint);
        g_mutex_unlock(&self->lock);
        return TRUE;
    }

    XTestFakeKeyEvent(self->display, keycode, release ? False : True, CurrentTime);
    XFlush(self->display);

    g_mutex_unlock(&self->lock);
    return TRUE;
}

/*
 * 功能：将指针标志转换为 XTest 按键 ID（按下为正，抬起为负）。
 * 逻辑：检查标志位是否匹配，决定返回正/负 button_id 或 0。
 * 参数：flags RDP 指针标志；mask 目标掩码；button_id XTest 按钮编号。
 * 外部接口：无（内部计算）。
 */
static int
drd_x11_input_pointer_button(guint16 flags, guint16 mask, int button_id)
{
    if ((flags & mask) == 0)
    {
        return 0;
    }

    const gboolean press = (flags & PTR_FLAGS_DOWN) != 0;
    return press ? button_id : -button_id;
}

/*
 * 功能：注入指针移动/按键/滚轮事件。
 * 逻辑：持锁检查运行态；按流/桌面尺寸计算缩放与裁剪后的坐标；根据标志注入移动、按键与滚轮事件，最后刷新 X11 输出。
 * 参数：self 输入实例；flags RDP 指针标志；x/y 流坐标；error 错误输出。
 * 外部接口：XTestFakeMotionEvent/XTestFakeButtonEvent/XFlush；依赖 GLib MAX 宏；日志 DRD_LOG_DEBUG。
 */
gboolean
drd_x11_input_inject_pointer(DrdX11Input *self,
                             guint16 flags,
                             guint16 x,
                             guint16 y,
                             GError **error)
{
    g_return_val_if_fail(DRD_IS_X11_INPUT(self), FALSE);

    g_mutex_lock(&self->lock);
    if (!drd_x11_input_check_running(self, error))
    {
        g_mutex_unlock(&self->lock);
        return FALSE;
    }

    const guint32 stream_width = MAX(self->stream_width, 1u);
    const guint32 stream_height = MAX(self->stream_height, 1u);
    const guint32 desktop_width = MAX(self->desktop_width, 1u);
    const guint32 desktop_height = MAX(self->desktop_height, 1u);

    const guint16 max_stream_x = (guint16)(stream_width > 0 ? stream_width - 1 : 0);
    const guint16 max_stream_y = (guint16)(stream_height > 0 ? stream_height - 1 : 0);
    const guint16 clamped_stream_x = x > max_stream_x ? max_stream_x : x;
    const guint16 clamped_stream_y = y > max_stream_y ? max_stream_y : y;

    guint16 target_x = clamped_stream_x;
    guint16 target_y = clamped_stream_y;
    if (stream_width != desktop_width)
    {
        guint scaled = (guint)((gdouble) clamped_stream_x * self->stream_to_desktop_scale_x + 0.5);
        if (scaled >= desktop_width)
        {
            scaled = desktop_width - 1;
        }
        target_x = (guint16) scaled;
    }
    if (stream_height != desktop_height)
    {
        guint scaled = (guint)((gdouble) clamped_stream_y * self->stream_to_desktop_scale_y + 0.5);
        if (scaled >= desktop_height)
        {
            scaled = desktop_height - 1;
        }
        target_y = (guint16) scaled;
    }

    if (flags & PTR_FLAGS_MOVE)
    {
        XTestFakeMotionEvent(self->display, self->screen, target_x, target_y, CurrentTime);
    }

    struct ButtonMask
    {
        guint16 mask;
        int button_id;
    } button_map[] = {
                {PTR_FLAGS_BUTTON1, 1},
                {PTR_FLAGS_BUTTON3, 2},
                {PTR_FLAGS_BUTTON2, 3},
            };

    for (guint i = 0; i < G_N_ELEMENTS(button_map); ++i)
    {
        int button_event = drd_x11_input_pointer_button(flags, button_map[i].mask, button_map[i].button_id);
        if (button_event > 0)
        {
            XTestFakeButtonEvent(self->display, button_event, True, CurrentTime);
        }
        else if (button_event < 0)
        {
            XTestFakeButtonEvent(self->display, -button_event, False, CurrentTime);
        }
    }

    if (flags & PTR_FLAGS_WHEEL)
    {
        const gboolean negative = (flags & PTR_FLAGS_WHEEL_NEGATIVE) != 0;
        const int button = negative ? 5 : 4;
        XTestFakeButtonEvent(self->display, button, True, CurrentTime);
        XTestFakeButtonEvent(self->display, button, False, CurrentTime);
    }

    if (flags & PTR_FLAGS_HWHEEL)
    {
        const gboolean negative = (flags & PTR_FLAGS_WHEEL_NEGATIVE) != 0;
        const int button = negative ? 7 : 6;
        XTestFakeButtonEvent(self->display, button, True, CurrentTime);
        XTestFakeButtonEvent(self->display, button, False, CurrentTime);
    }

    XFlush(self->display);
    g_mutex_unlock(&self->lock);
    return TRUE;
}

/*
 * 功能：根据特殊扫描码映射 X11 KeyCode（处理左右 Ctrl/Alt/Shift/Win）。
 *
 * 详细说明：
 * 该函数用于处理修饰键（Ctrl/Alt/Shift/Win）的特殊映射逻辑。
 * 在 RDP 协议中，左右修饰键使用相同的扫描码，通过 extended 标志来区分：
 * - extended = FALSE: 表示左键
 * - extended = TRUE: 表示右键
 *
 * 例如：
 * - RDP_SCANCODE_LMENU + extended=FALSE -> 左 Alt (XK_Alt_L)
 * - RDP_SCANCODE_LMENU + extended=TRUE -> 右 Alt (XK_Alt_R)
 *
 * 映射逻辑：
 * - 对于修饰键扫描码（LMENU/LCONTROL/LSHIFT/LWIN）：
 *   - extended = FALSE: 映射到左键（XK_Alt_L/XK_Control_L/XK_Shift_L/XK_Super_L）
 *   - extended = TRUE: 映射到右键（XK_Alt_R/XK_Control_R/XK_Shift_R/XK_Super_R）
 *
 * 参数：
 *   self - 输入实例指针，包含 X11 display 连接
 *   scancode - RDP 基础扫描码（0-255）
 *   extended - 是否为扩展键标志，用于区分左右修饰键
 *
 * 返回值：
 *   成功时返回对应的 X11 keycode（非零值）
 *   失败时返回 0（表示无法映射该扫描码）
 *
 * 外部接口：
 *   - X11: XKeysymToKeycode（KeySym 到 keycode 转换）
 *   - X11: KeySym 常量（XK_Alt_L, XK_Alt_R, XK_Control_L 等）
 */
static KeyCode
drd_x11_input_lookup_modifier_keycode(DrdX11Input *self, guint8 scancode, gboolean extended)
{
    // 检查 display 是否有效
    if (self->display == NULL)
    {
        return 0;
    }

    KeySym keysym = NoSymbol;

    // 根据 RDP 扫描码选择对应的 X11 KeySym
    // 注意：在 RDP 协议中，左右修饰键使用相同的扫描码，通过 extended 标志区分
    switch (scancode)
    {
        case RDP_SCANCODE_CODE(RDP_SCANCODE_LMENU):
            // LMENU: extended=FALSE -> 左 Alt, extended=TRUE -> 右 Alt
            keysym = extended ? XK_Alt_R : XK_Alt_L;
            break;
        case RDP_SCANCODE_CODE(RDP_SCANCODE_LCONTROL):
            // LCONTROL: extended=FALSE -> 左 Ctrl, extended=TRUE -> 右 Ctrl
            keysym = extended ? XK_Control_R : XK_Control_L;
            break;
        case RDP_SCANCODE_CODE(RDP_SCANCODE_LSHIFT):
            // LSHIFT: extended=FALSE -> 左 Shift, extended=TRUE -> 右 Shift
            keysym = extended ? XK_Shift_R : XK_Shift_L;
            break;
        case RDP_SCANCODE_CODE(RDP_SCANCODE_LWIN):
            // LWIN: extended=FALSE -> 左 Win, extended=TRUE -> 右 Win
            keysym = extended ? XK_Super_R : XK_Super_L;
            break;
        default:
            // 不支持的扫描码
            break;
    }

    // 如果没有找到对应的 KeySym，返回 0
    if (keysym == NoSymbol)
    {
        return 0;
    }

    // 将 KeySym 转换为 X11 keycode
    // 使用 XKeysymToKeycode 可以确保与当前系统的键盘布局一致
    KeyCode keycode = XKeysymToKeycode(self->display, keysym);
    return keycode;
}

/*
 * 功能：重置扫描码到 keycode 的缓存表。
 * 逻辑：遍历缓存数组写入无效标记。
 * 参数：self 输入实例。
 * 外部接口：无。
 */
static void
drd_x11_input_reset_keycode_cache(DrdX11Input *self)
{
    for (guint i = 0; i < DRD_X11_KEYCODE_CACHE_SIZE; ++i)
    {
        self->keycode_cache[i] = DRD_X11_KEYCODE_CACHE_INVALID;
    }
}

/*
 * 功能：解析 RDP 扫描码对应的 X11 keycode，并可告知是否发生缓存未命中。
 *
 * 详细说明：
 * 该函数是键盘输入转换的核心组件，负责将远程桌面协议（RDP）的键盘扫描码映射到本地 X11 系统的 keycode。
 * 由于 RDP 和 X11 使用不同的键盘编码体系，需要进行转换。为了提高性能，函数实现了缓存机制，
 * 避免每次按键事件都进行昂贵的映射计算。
 *
 * 缓存策略：
 * - 使用 512 个元素的缓存数组（DRD_X11_KEYCODE_CACHE_SIZE）
 * - 索引计算：base_scancode + (extended ? 256 : 0)
 *   - 非扩展键（0-255）：索引范围 0-255
 *   - 扩展键（0-255）：索引范围 256-511
 * - 缓存未命中时，通过 FreeRDP 库进行映射，失败则尝试特殊键查找
 * - 映射结果存入缓存，后续相同按键直接返回缓存值
 *
 * 映射流程：
 * 1. 计算缓存索引（考虑扩展标志）
 * 2. 检查缓存是否命中
 * 3. 缓存未命中时：
 *    a. 对于修饰键（Ctrl/Alt/Shift/Win），优先使用 X11 的 XKeysymToKeycode 进行映射
 *       这是因为 FreeRDP 的键盘映射表可能与当前系统的 X11 键盘布局不一致
 *    b. 对于其他按键，使用 FreeRDP 的 freerdp_keyboard_get_x11_keycode_from_scancode 进行标准映射
 *    c. 如果标准映射失败（返回 0），尝试特殊键映射
 *    d. 将映射结果存入缓存
 * 4. 返回映射结果，并通过 out_cache_miss 参数告知调用者是否发生缓存未命中
 *
 * 参数：
 *   self - 输入实例指针，包含 X11 display 连接和缓存数组
 *   base_scancode - RDP 基础扫描码（0-255），表示按键的物理位置
 *   extended - 是否为扩展键标志，用于区分左右修饰键（如左 Ctrl vs 右 Ctrl）
 *   out_cache_miss - 输出参数，用于返回是否发生缓存未命中（可选，可为 NULL）
 *
 * 返回值：
 *   成功时返回对应的 X11 keycode（非零值）
 *   失败时返回 0（表示无法映射该扫描码）
 *
 * 外部接口：
 *   - FreeRDP: freerdp_keyboard_get_x11_keycode_from_scancode（标准扫描码映射）
 *   - 内部: drd_x11_input_lookup_modifier_keycode（特殊键映射）
 *   - GLib: g_return_val_if_fail（参数校验）
 *
 * 使用场景：
 *   在 drd_x11_input_inject_keyboard 函数中被调用，用于将 RDP 键盘事件转换为 X11 事件。
 *   每次按键事件都会调用此函数，因此缓存机制对性能至关重要。
 */
static guint16
drd_x11_input_resolve_keycode(DrdX11Input *self,
                              guint8 base_scancode,
                              gboolean extended,
                              gboolean *out_cache_miss)
{
    // 参数校验：确保 self 是有效的 DrdX11Input 实例
    g_return_val_if_fail(DRD_IS_X11_INPUT(self), 0);

    // 计算缓存索引：
    // - 非扩展键：索引 = base_scancode（范围 0-255）
    // - 扩展键：索引 = base_scancode + 256（范围 256-511）
    // 这样设计可以区分同一扫描码的扩展和非扩展版本（如左 Ctrl 和右 Ctrl）
    const guint index = base_scancode + (extended ? 256u : 0u);

    // 边界检查：确保索引在缓存数组范围内
    g_return_val_if_fail(index < DRD_X11_KEYCODE_CACHE_SIZE, 0);

    // 从缓存中读取已存储的 keycode
    guint16 cached = self->keycode_cache[index];

    // 标记是否发生缓存未命中，默认为 FALSE
    gboolean cache_miss = FALSE;

    // 检查缓存是否有效（DRD_X11_KEYCODE_CACHE_INVALID = 0xFFFF 表示无效）
    if (cached == DRD_X11_KEYCODE_CACHE_INVALID)
    {
        // 缓存未命中，需要进行映射计算
        cache_miss = TRUE;

        guint16 keycode = 0;

        // 步骤 1：对于修饰键（Ctrl/Alt/Shift/Win），优先使用 X11 的 XKeysymToKeycode 进行映射
        // 这是因为 FreeRDP 的键盘映射表可能与当前系统的 X11 键盘布局不一致
        // 例如：FreeRDP 可能返回左 Alt 的 keycode 为 205，但实际系统的 keycode 是 64
        // 注意：在 RDP 协议中，左右修饰键使用相同的扫描码，通过 extended 标志区分
        switch (base_scancode)
        {
            case RDP_SCANCODE_CODE(RDP_SCANCODE_LMENU):    // 左/右 Alt（通过 extended 标志区分）
            case RDP_SCANCODE_CODE(RDP_SCANCODE_LCONTROL): // 左/右 Ctrl（通过 extended 标志区分）
            case RDP_SCANCODE_CODE(RDP_SCANCODE_LSHIFT):   // 左/右 Shift（通过 extended 标志区分）
            case RDP_SCANCODE_CODE(RDP_SCANCODE_LWIN):     // 左/右 Win（通过 extended 标志区分）
                // 对于修饰键，使用 X11 的 KeySym 到 keycode 映射
                // 这样可以确保与当前系统的键盘布局一致
                keycode = (guint16) drd_x11_input_lookup_modifier_keycode(self,
                                                                         base_scancode,
                                                                         extended);
                break;
            default:
                // 步骤 2：对于其他按键，使用 FreeRDP 的标准映射函数
                // 该函数根据 RDP 扫描码和扩展标志返回对应的 X11 keycode
                keycode = (guint16) freerdp_keyboard_get_x11_keycode_from_scancode(
                        base_scancode, extended ? TRUE : FALSE);

                // 步骤 3：如果标准映射失败（返回 0），尝试特殊键映射
                if (keycode == 0)
                {
                    keycode = (guint16) drd_x11_input_lookup_modifier_keycode(self,
                                                                             base_scancode,
                                                                             extended);
                }
                break;
        }

        // 步骤 4：将映射结果存入缓存，供后续使用
        // 即使映射失败（keycode = 0），也会缓存该结果，避免重复计算
        self->keycode_cache[index] = keycode;
        cached = keycode;
    }

    // 如果调用者提供了 out_cache_miss 参数，则返回缓存未命中状态
    // 这对于性能监控和调试很有用
    if (out_cache_miss != NULL)
    {
        *out_cache_miss = cache_miss;
    }

    // 返回映射结果（可能是缓存值或新计算的值）
    return cached;
}

/*
 * 功能：刷新流坐标到桌面坐标的缩放因子。
 * 逻辑：根据流尺寸与桌面尺寸计算 x/y 缩放，避免除零。
 * 参数：self 输入实例。
 * 外部接口：无。
 */
static void
drd_x11_input_refresh_pointer_scale(DrdX11Input *self)
{
    const guint32 stream_width = self->stream_width > 0 ? self->stream_width : 1u;
    const guint32 stream_height = self->stream_height > 0 ? self->stream_height : 1u;
    const guint32 desktop_width = self->desktop_width > 0 ? self->desktop_width : 1u;
    const guint32 desktop_height = self->desktop_height > 0 ? self->desktop_height : 1u;

    self->stream_to_desktop_scale_x =
            (stream_width == desktop_width)
                ? 1.0
                : ((gdouble) desktop_width / (gdouble) stream_width);
    self->stream_to_desktop_scale_y =
            (stream_height == desktop_height)
                ? 1.0
                : ((gdouble) desktop_height / (gdouble) stream_height);
}

/*
 * 功能：将 Unicode 码点转换为 X11 KeySym。
 * 逻辑：处理常见控制字符，ASCII 直接返回；合法 BMP/非 BMP 码点附加 0x01000000 标志；非法返回 NoSymbol。
 * 参数：codepoint Unicode 码点。
 * 外部接口：X11 KeySym 常量。
 */
static KeySym
drd_x11_input_keysym_from_codepoint(gunichar codepoint)
{
    switch (codepoint)
    {
        case '\r':
            return XK_Return;
        case '\n':
            return XK_Linefeed;
        case '\t':
            return XK_Tab;
        case '\b':
            return XK_BackSpace;
        default:
            break;
    }

    if (codepoint <= 0xFF)
    {
        return (KeySym) codepoint;
    }

    if (codepoint > 0 && codepoint <= 0x10FFFF)
    {
        return (KeySym) (codepoint | 0x01000000);
    }

    return NoSymbol;
}
