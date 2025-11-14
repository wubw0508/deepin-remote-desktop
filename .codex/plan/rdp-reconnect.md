# RDP 重连故障排查

## 上下文
- 首次连接能够完成握手，断开后再次连接失败，日志显示 `session already active`，提示会话未被释放。
- 需确保 FreeRDP 会话生命周期正确回收，并允许后续客户端创建新连接。

## 计划
1. [x] 审阅 `DrdRdpSession` 与 `DrdRdpListener` 的会话管理代码，确认断线未清理的根因及影响范围。
2. [x] 为断线场景补充会话清理逻辑（监听器会话表更新、上下文引用回收），保证不会拒绝新的连接。
3. [x] 补充必要的单元测试或最小验证（如构建/静态检查），确保新逻辑可靠；必要时记录无法测试的原因。
4. [x] 更新 `doc/architecture.md` 与 `doc/changelog.md`，同步变更意图与影响，并征求用户反馈。

## 二次排查计划（断线未调用 `drd_peer_disconnected`）
1. [x] 梳理 FreeRDP 回调触发链，确认 session 内部在哪些路径可能提前终止而未经过 peer 回调。
2. [x] 在 `DrdRdpSession` 层增加一次性关闭回调，并由监听器注册，确保任何主动断开都能及时移除会话。
3. [x] 重新编译并观察运行日志，验证无编译回归；若需要，指导人工触发断线测试。
4. [x] 更新计划/文档说明新增回调语义，并请用户复测。

## 进度
- [x] 读完 `drd_rdp_listener.c`/`drd_rdp_session.c`，确认监听器仅在 stop 时才清空 `sessions` 数组，导致断线后仍被视为“active”。
- [x] 为 `DrdRdpPeerContext` 增加 listener 引用，断线/析构时统一调用 `drd_rdp_listener_session_closed` 移除会话并记录日志。
- [x] 运行 `meson compile -C build` 验证无编译回归；目前会话管理依赖 FreeRDP 内部结构，缺少可注入的 seam 难以在 GLib 测试中构造 `freerdp_peer`，因此暂未新增自动化用例。
- [x] 更新 `doc/architecture.md`/`doc/changelog.md`，新增会话生命周期描述与任务记录，待用户确认
