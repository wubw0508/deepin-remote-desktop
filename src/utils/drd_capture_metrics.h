#pragma once

#include <glib.h>

/*
 * 采集/渲染帧率观测配置，默认 60fps、5 秒窗口，可由配置层调用 apply 接口覆盖。
 * 目标帧率/统计窗口从 config 读取，省去环境变量依赖，便于统一配置管理。
 */
void drd_capture_metrics_apply_config(guint target_fps, guint stats_interval_sec);

guint drd_capture_metrics_get_target_fps(void);
gint64 drd_capture_metrics_get_target_interval_us(void);
gint64 drd_capture_metrics_get_stats_interval_us(void);
