# 任务：监听层切换至 GSocketService

## 上下文
- upstream `GrdRdpServer` 通过继承 `GSocketService` 完成端口绑定和来连接分发，可以进入 system/handover 模式。
- 当前 `DrdRdpListener` 基于 `freerdp_listener`，通过轮询 `CheckFileDescriptor` 接受连接，无法支持 handover。
- 目标是复用 upstream 思路，将 deepin listener 改造成 GSocketService 驱动，并让会话层直接从 `GSocketConnection` 构造 `DrdRdpSession`，暂不实现 takeclient/DBus。

## 计划
1. **类型重构**：让 `DrdRdpListener` 继承 `GSocketService`，调整结构体字段与生命周期，移除 `freerdp_listener` 依赖。
2. **端口绑定**：实现基于 `g_socket_listener_add_*` 的绑定函数，结合 system 模式布尔控制，准备 `GCancellable` 钩子。
3. **会话封装**：在监听层完成 `GSocketConnection` → `freerdp_peer` 转换，复用既有 TLS/NLA/输入初始化逻辑。
4. **应用集成**：更新 `drd_application_start_listener()` 等路径，调用新的 start/stop API，确保日志与错误处理齐备。
5. **文档更新**：在 `doc/architecture.md` 描述新的监听架构，并在 `doc/changelog.md` 记录改动影响。

> 关键步骤完成后需要回报用户确认，并在执行完毕进入优化/评审阶段。

## 进度
- [x] 类型重构
- [x] 端口绑定
- [x] 会话封装（listener 内部完成 GSocketConnection→freerdp_peer 转换）
- [x] 应用集成（确认启动/停止路径兼容新 listener）
- [x] 文档更新
