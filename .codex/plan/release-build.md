# 任务：release-build

## 背景
- 当前 `build/` 目录按照默认 `meson setup` 生成，`buildtype` 为 `debug`，产物包含 `debug_info` 且未 strip。
- 目标是给出生成 Release 产物的可执行方案，遵循 Meson 工作流及仓库指南。

## 计划
1. [x] 复查项目构建约束与 Meson 约定，确认可调参数（`AGENTS.md`、`meson.build`）。
2. [x] 设计 Release 构建参数组合（`buildtype`、`optimization`、`debug`、`strip` 等）。
3. [x] 输出可操作步骤，包括全新 build 目录与增量切换两种路径，提醒相关验证命令。

## 遗留/风险
- 尚未向用户确认 Release 工具链偏好（如是否启用 LTO、PGO），后续需按反馈微调。
