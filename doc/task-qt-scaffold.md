# 任务：Qt 框架骨架与 Meson 接入

## 目的
在保持 Meson 构建的前提下，引入 Qt6 依赖并搭建分模块的 Qt 框架骨架，为后续替换 GLib 主循环与 D-Bus 逻辑做准备。

## 范围
- 构建系统：`meson.build`、`src/meson.build`、`qt/meson.build`。
- Qt 目录：`qt/` 及其子模块骨架（core/session/transport/security/system）。
- 入口：`src/main.cpp`（Qt 化入口）。

## 修改内容
1. 新增 `qt/` 目录并按模块放置 Qt 桥接骨架类。
2. Meson 引入 Qt6 依赖，新增 Qt 模块静态库并挂接至可执行文件。
3. 入口程序改用 QtCore 应用对象完成 Qt 化编译。

## 影响
- 构建系统仍以 Meson 为主，但新增 Qt 依赖与 C++ 编译路径。
- 运行逻辑保持不变，仅引入 Qt 骨架以便后续分阶段迁移。
