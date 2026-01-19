# 任务：入口改为 Qt Core Application

## 目的
- 将程序入口迁移至 Qt Core Application，便于后续 Qt 化扩展，同时保持现有运行逻辑不变。

## 范围
- `src/main.cpp`
- `meson.build`
- `src/meson.build`

## 变更
- 入口文件改为 C++，初始化 `QCoreApplication` 后继续调用现有 `drd_application_run`。
- Meson 构建启用 C++ 并加入 Qt6 Core 依赖，保持其余源文件与库链接不变。

## 影响
- 主程序具备 Qt Core 初始化能力；现有 GLib/FreeRDP 逻辑与运行路径保持一致。
