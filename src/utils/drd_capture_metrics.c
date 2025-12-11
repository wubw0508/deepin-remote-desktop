#include "utils/drd_capture_metrics.h"

#include <glib.h>

static gsize metrics_init_once = 0;
static guint cached_target_fps = 60;
static gint64 cached_target_interval_us = G_USEC_PER_SEC / 60;
static gint64 cached_stats_interval_us = (gint64) 5 * G_USEC_PER_SEC;

/*
 * 功能：将配置层提供的帧率与统计窗口写入缓存，带上边界保护。
 * 逻辑：若参数为 0 则回落默认值；fps 限制在 1-240，窗口秒数限制在 1-60，并同步计算微秒间隔。
 * 参数：target_fps 目标帧率；stats_interval_sec 统计窗口秒数。
 * 外部接口：无，供配置初始化阶段调用一次。
 */
void
drd_capture_metrics_apply_config(guint target_fps, guint stats_interval_sec)
{
    if (target_fps == 0)
    {
        target_fps = 60;
    }
    if (stats_interval_sec == 0)
    {
        stats_interval_sec = 5;
    }

    if (target_fps < 1)
    {
        target_fps = 1;
    }
    else if (target_fps > 240)
    {
        target_fps = 240;
    }

    if (stats_interval_sec < 1)
    {
        stats_interval_sec = 1;
    }
    else if (stats_interval_sec > 60)
    {
        stats_interval_sec = 60;
    }

    cached_target_fps = target_fps;
    cached_target_interval_us = (gint64) (G_USEC_PER_SEC / cached_target_fps);
    cached_stats_interval_us = (gint64) stats_interval_sec * G_USEC_PER_SEC;

    if (g_once_init_enter(&metrics_init_once))
    {
        g_once_init_leave(&metrics_init_once, 1);
    }
}

guint
drd_capture_metrics_get_target_fps(void)
{
    /*
     * 功能：获取目标帧率。
     * 逻辑：返回配置缓存的 target_fps，未配置时为默认 60。
     * 参数：无。
     * 外部接口：无。
     */
    return cached_target_fps;
}

gint64
drd_capture_metrics_get_target_interval_us(void)
{
    /*
     * 功能：获取目标帧间隔（微秒）。
     * 逻辑：基于当前 target_fps 计算得到的缓存值。
     * 参数：无。
     * 外部接口：无。
     */
    return cached_target_interval_us;
}

gint64
drd_capture_metrics_get_stats_interval_us(void)
{
    /*
     * 功能：获取帧率统计窗口长度（微秒）。
     * 逻辑：返回配置缓存的统计窗口，默认 5 秒。
     * 参数：无。
     * 外部接口：无。
     */
    return cached_stats_interval_us;
}
