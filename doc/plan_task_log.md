# 计划任务记录

## 架构设计概览
- 项目整体架构详见 `doc/architecture.md`，该文档覆盖核心层、采集层、编码层、传输层与运行模式的模块边界与依赖关系。

## 2026-01-08：虚拟机 H264 开关
- **目的**：提供配置项控制虚拟机环境是否启用 H264，避免虚拟化场景下 H264 协议导致的兼容问题。
- **范围**：`src/core/drd_config.c`、`src/core/drd_encoding_options.h`、`src/transport/drd_rdp_listener.c`、`src/utils/drd_system_info.*`、`data/config.d/*.ini`。
- **修改内容**：新增 `h264_vm_support` 配置字段，运行时检测虚拟机并在配置关闭时禁用 `FreeRDP_GfxH264`。
- **影响**：默认行为保持为虚拟机不启用 H264，避免不稳定的图形管线协商；显式开启后可继续使用 H264 编码。

## 2026-03-12：Qt 框架骨架与 Meson 接入
- **目的**：在不切换构建系统的前提下建立 Qt 迁移骨架。
- **范围**：`qt/`、`meson.build`、`src/meson.build`、`src/main.cpp`、`doc/task-qt-scaffold.md`。
- **修改内容**：新增 Qt 模块骨架类，Meson 引入 Qt6 依赖并链接静态库，入口改用 QtCore 进行初始化。
- **影响**：构建流程仍使用 Meson，但新增 Qt/C++ 编译链，为后续模块迁移提供基础。
