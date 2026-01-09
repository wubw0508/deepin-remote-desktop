#include "utils/drd_system_info.h"

#include <gio/gio.h>

/*
 * 功能：检测文本中是否包含虚拟化标识关键字。
 * 逻辑：转换为小写后查找常见厂商/平台标识，命中即返回 TRUE。
 * 参数：text 待检测文本。
 * 外部接口：GLib g_ascii_strdown/g_strstr_len。
 */
static gboolean
drd_system_text_contains_vm_marker(const gchar *text)
{
    if (text == NULL)
    {
        return FALSE;
    }

    static const gchar *markers[] = {
        "kvm",
        "qemu",
        "vmware",
        "virtualbox",
        "hyper-v",
        "hyperv",
        "xen",
        "bhyve",
        "bochs",
        "parallels",
        "rhev",
        "ovirt",
        "openstack",
        "virtual machine"
    };

    g_autofree gchar *lower = g_ascii_strdown(text, -1);
    if (lower == NULL)
    {
        return FALSE;
    }

    for (guint i = 0; i < G_N_ELEMENTS(markers); i++)
    {
        if (g_strstr_len(lower, -1, markers[i]) != NULL)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/*
 * 功能：读取指定文件并检查虚拟化标识。
 * 逻辑：读取文本内容并调用关键字匹配函数，读取失败视为未命中。
 * 参数：path 文件路径。
 * 外部接口：GLib g_file_get_contents。
 */
static gboolean
drd_system_file_contains_vm_marker(const gchar *path)
{
    if (path == NULL)
    {
        return FALSE;
    }

    g_autofree gchar *content = NULL;
    gsize length = 0;
    if (!g_file_get_contents(path, &content, &length, NULL))
    {
        return FALSE;
    }

    return drd_system_text_contains_vm_marker(content);
}

/*
 * 功能：判断当前系统是否运行在虚拟机内。
 * 逻辑：读取 DMI/CPU 信息关键字判断，结果缓存以减少重复 IO。
 * 参数：无。
 * 外部接口：GLib g_once_init_enter/g_once_init_leave。
 */
gboolean
drd_system_is_virtual_machine(void)
{
    static gsize initialized = 0;
    static gboolean cached = FALSE;

    if (g_once_init_enter(&initialized))
    {
        cached = drd_system_file_contains_vm_marker("/sys/class/dmi/id/product_name") ||
                 drd_system_file_contains_vm_marker("/sys/class/dmi/id/sys_vendor") ||
                 drd_system_file_contains_vm_marker("/proc/cpuinfo");
        g_once_init_leave(&initialized, 1);
    }

    return cached;
}
