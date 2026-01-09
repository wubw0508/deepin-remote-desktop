#pragma once

#include <glib.h>

G_BEGIN_DECLS

/*
 * 功能：判断当前系统是否运行在虚拟机内。
 * 逻辑：基于 DMI 与 CPU 信息关键字检测常见虚拟化标识，缓存首次判断结果。
 * 参数：无。
 * 外部接口：GLib g_file_get_contents/g_ascii_strdown/g_once_init_enter。
 */
gboolean drd_system_is_virtual_machine(void);

G_END_DECLS
