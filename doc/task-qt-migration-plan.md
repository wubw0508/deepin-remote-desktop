# 任务：Qt 化与 CMake 构建迁移方案（阶段一）

## 目的
梳理现有 deepin-remote-desktop 的模块边界与职责，为后续 Qt 化与 CMake 构建迁移提供可落地的拆分方案与迁移顺序。该文档仅做模块划分与迁移步骤规划，不涉及代码实现改动。

## 范围
- 源码目录：`src/` 全量模块（捕获、编码、输入、核心、会话、传输、安全、系统、工具）。
- 迁移目录：新增 `qt/` 作为 Qt 化迁移的独立承载路径。
- 构建系统：优先使用 Meson（Qt6），CMake 仅作为远期目标拆分方案。
- 文档/配置：保持现有配置与证书目录结构，迁移过程中优先复用。

## 现有模块区分（基于目录职责）
1. **采集/编码/输入（libdrd-media 侧）**
   - `src/capture`：屏幕采集、帧缓存、脏矩形等基础采集逻辑。
   - `src/encoding`：视频/图像编码器管理、编码参数与数据管线。
   - `src/input`：键鼠注入、按键映射、输入设备协同。
   - `src/utils`：队列、时钟、统计、通用工具与线程同步等。
2. **运行时核心与会话层**
   - `src/core`：配置解析、运行时上下文、服务启动与生命周期管理。
   - `src/session`：RDP 会话状态机、图形管线、媒体协商。
3. **传输与安全**
   - `src/transport`：RDP 监听、连接管理、通道与握手调度。
   - `src/security`：认证、TLS/NLA 钩子与安全策略。
4. **系统集成与入口**
   - `src/system`：系统级能力探测、系统信息与宿主集成。
   - `src/main.c`：进程入口、参数解析、启动引导。
   - D-Bus 描述：`src/org.deepin.RemoteDesktop*.xml`、`src/org.deepin.DisplayManager.xml`。

## Qt 化目标模块拆分建议
1. **QtCore/QtNetwork 层（服务内核）**
   - 迁移 `core` 的配置解析、运行时上下文与服务生命周期到 Qt 事件循环。
   - 将现有 GLib 主循环替换为 `QCoreApplication` + `QEventLoop`。
   - Qt 相关文件集中放置在 `qt/`，按模块划分子目录。
2. **媒体管线层（保持 C/C++ 实现）**
   - `capture`/`encoding`/`input`/`utils` 作为独立静态库或对象库导入 CMake。
   - 逐步消减 GLib 依赖：统一替换线程/锁/定时器/主循环为 Qt（Qt6）或 C++ 标准库实现。
3. **会话与协议层**
   - `session`/`transport` 仍保持 C/C++ 实现，分离协议相关逻辑与 Qt glue。
   - 现有 GLib 回调/信号机制通过 Qt 信号槽或事件循环替换，减少核心协议重写。
4. **安全与系统集成**
   - `security` 维持独立模块，Qt 侧通过接口注入证书/认证配置。
   - `system` 保持系统探测逻辑，Qt 侧读取结果并上报状态。
5. **入口与 D-Bus**
   - 新增 Qt 入口（C++ `main.cpp`），逐步替换 GLib 主循环。
   - 使用 QtDBus 接管 D-Bus 服务暴露，XML 继续复用。

## 构建系统建议（以 Meson 为主）
1. **Meson 过渡构建**
   - 在 Meson 中引入 Qt6 依赖，新增 `qt/` 子目录构建最小 Qt 框架模块。
   - 先确保 Qt 入口可编译并与现有 C 模块联动，再逐步替换 GLib 逻辑。
2. **CMake 远期结构**
   - 保留 CMake 作为远期迁移目标，后续在 Meson 稳定后再启动。

## CMake 迁移与构建结构建议（远期）
1. **顶层 `CMakeLists.txt`**
   - 定义基础选项（Qt 版本、编译选项、开关宏）。
   - 集中管理依赖（FreeRDP/FFmpeg/GLib 的过渡期链接）。
2. **子目录拆分**
   - `src/capture`、`src/encoding`、`src/input`、`src/utils`：先构建为 `drd-media` 静态库。
   - `src/core`、`src/session`、`src/transport`、`src/security`、`src/system`：构建为 `drd-runtime` 静态/共享库。
   - `src` 根目录：生成 Qt 入口二进制 `deepin-remote-desktop-qt`。
3. **过渡期策略**
   - 先保留 Meson 构建，CMake 与 Meson 并行；确保功能一致后切换默认。
   - 逐步替换 GLib 依赖点，减少 CMake 中的 GLib 链接范围。

## 迁移阶段建议（第一阶段聚焦方案与边界）
1. **阶段 0：模块盘点与依赖图**
   - 输出模块依赖清单（现有 `src/*` 目录与外部库依赖）。
   - 明确 Qt 替代项：事件循环、线程、定时器、配置与日志。
2. **阶段 1：CMake 构建骨架**
   - 先生成空白 Qt 入口与模块静态库，保证可编译链接。
   - 确认与 Meson 的产物对齐（可执行文件、静态库命名）。
3. **阶段 2：Qt glue 与主循环切换**
   - 将 `core` 的主循环与启动逻辑迁移到 Qt，保留原 C API。
   - 引入 QtDBus 接口层，完成服务注册与生命周期管理。
4. **阶段 3：逐步替换 GLib 依赖**
   - 优先替换工具与线程模块，减少 `utils` 中的 GLib 依赖。
   - 再处理 `session`/`transport` 中的主循环与回调调度。

## 影响与风险
- 初期将同时维护 Meson 与 CMake 构建，需保证配置同步。
- Qt 事件循环替换可能影响线程与定时器行为，需要补充回归验证。
- D-Bus 切换到 QtDBus 后需要验证与现有客户端的协议一致性。

## 下一步
- 建议补充模块依赖图（可基于当前 `meson.build` 与头文件依赖整理）。
- 在完成 CMake 骨架后，建立最小 Qt 入口与日志输出，以便验证运行流程。
