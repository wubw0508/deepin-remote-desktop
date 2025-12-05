## SUSPEND_FRAME_ACKNOWLEDGEMENT 问题
  
  - suspend_frame_acknowledgement 由 handle_frame_ack_event 在收到 RDPGFX_FRAME_ACKNOWLEDGE_PDU 且 queueDepth == SUSPEND_FRAME_ACKNOWLEDGEMENT 时触发，用来实现 [MS-RDPEGFX] 中“挂起帧确认”语义，避免服务器继续等待逐帧 ACK（src/grd-rdp-graphics-pipeline.c:1834-1879）。
  - 函数首先将 frame_acks_suspended 置为 TRUE，下游编码路径会据此立即自确认新帧而不是等待客户端（src/grd-rdp-graphics-pipeline.c:1824 与 src/grd-rdp-graphics-pipeline.c:935-963）。
  - 该逻辑保护 frame_serial_table（实现“Unacknowledged Frames ADM element”）和 encoded_frames 队列，确保我们能够在挂起后按 totalFramesDecoded 重新构建历史（src/grd-rdp-graphics-
    pipeline.c:115-133, src/grd-rdp-graphics-pipeline.c:732-770）。
  - 通过这一机制，服务器能在客户端压力过大时迅速释放历史状态，同时保持足够的上下文以便稍后 maybe_rewrite_frame_history 将状态复原（src/grd-rdp-graphics-pipeline.c:1758-1790）。

  内部步骤

  - 遍历 surface_table，对每个 GrdRdpGfxFrameController 执行 grd_rdp_gfx_frame_controller_clear_all_unacked_frames，立即丢弃所有尚未送达的帧记录，防止后续 ack 命中失效帧（src/grd-rdp-graphics-pipeline.c:1794-1806）。
  - 调用 reduce_tracked_frame_infos (..., 0) 清空 encoded_frames 队列，避免挂起期间继续积压历史，减轻内存压力（src/grd-rdp-graphics-pipeline.c:1829）。
  - 使用 g_hash_table_foreach_remove 将 frame_serial_table 的 frameId→surfaceSerial 映射全部抛弃，并在 frame_serial_free 中释放 surface serial 引用，保证 surface 生命周期计数正确（src/grd-rdp-graphics-pipeline.c:1808-1831）。
  - 挂起结束后首次收到非挂起 FrameAck 时，maybe_rewrite_frame_history 按 pending_frame_acks 数量重新把最近的编码帧写回 “未确认帧” 队列，弥合挂起期间的虚假确认（src/grd-rdp-graphics-
    pipeline.c:1758-1789）。

  流程关联与风险

  - 如果未在挂起开始时清空每个 frame_controller 内部的未确认帧，后续“自确认”将与客户端真实的 totalFramesDecoded 不一致，导致grd_rdp_gfx_frame_controller_ack_frame 的帧 id 复用甚至触发
    g_assert（src/grd-rdp-graphics-pipeline.c:1834-1876）。
  - frame_acks_suspended 还被 grd_rdp_graphics_pipeline_refresh_gfx、refresh_gfx_surface_avc420、grd_rdp_graphics_pipeline_refresh_gfx_surface_frame 等多处编码路径检查，用来决定是否立即 ack 并把帧信息放入 encoded_frames 以备恢复，因此挂起设置必须与队列清理同步，否则 enqueue_tracked_frame_info 会留着旧 serial 造成 use-after-free（src/grd-rdp-graphics-pipeline.c:935-963, src/grd-rdp-graphics-pipeline.c:1114-1132, src/grd-rdp-graphics-pipeline.c:1388-1411）。
  - 状态复原依赖 pending_frame_acks = total_frames_encoded - totalFramesDecoded，若挂起时未正确维护 total_frames_encoded（例如提前重置），maybe_rewrite_frame_history 可能减少过多历史，进而让 grd_rdp_gfx_frame_controller_unack_last_acked_frame 丢帧。
  - 因为 suspend_frame_acknowledgement 在持有 gfx_mutex 的上下文中执行，任何在挂起路径中阻塞的调用都可能冻结整个渲染线程；保持函数本身 O(N) 的复杂度对于大规模 surface 场景需要留意。
