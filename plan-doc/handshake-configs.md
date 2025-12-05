# RDP 握手配置清单

本文件罗列 GNOME Remote Desktop（48.0）在与 RDP 客户端握手及 PostConnect 阶段读取的全部 `FreeRDP_*` 配置以及服务端主动写入的配置，便于审计每个字段的行为。路径均为相对仓库根目录。

## 客户端配置读取

| 配置键 | 读取位置 | 影响 |
| --- | --- | --- |
| `FreeRDP_OsMajorType` | `src/grd-session-rdp.c:203-212`, `298-339`, `991-1037`, `1652-1670` | 识别 MSTSC 客户端、决定服务端在重定向时是否使用 PKCS 加密密码、判定移动端设备以禁用音频、推断剪贴板转换策略。 |
| `FreeRDP_OsMinorType` | `src/grd-session-rdp.c:203-212` | 与 `OsMajorType` 组合识别 Windows NT MSTSC，用于系统守护进程记录客户端类型。 |
| `FreeRDP_KeyboardType` | `src/grd-session-rdp.c:704-741` | 将客户端扫描码转换为 xkb keycode，驱动 GNOME 输入事件队列。 |
| `FreeRDP_SupportGraphicsPipeline` | `src/grd-session-rdp.c:873-1051`, `800-821` | 若未声明 GFX 能力直接拒绝连接；若声明却缺少网络自适应则给出警告；还决定是否在输出抑制时注册 RTT 消费者。 |
| `FreeRDP_RemoteFxCodec` / `FreeRDP_RemoteFxImageCodec` / `FreeRDP_NSCodec` | `src/grd-session-rdp.c:880-903` | 只要客户端声称支持任意编解码器，就要求其同时支持 32bpp，若 `SupportedColorDepths` 不含 32bpp 将拒绝连接。 |
| `FreeRDP_SupportedColorDepths` | `src/grd-session-rdp.c:886-893` | 验证客户端 32bpp 能力，缺失时立即断开。 |
| `FreeRDP_ColorDepth` | `src/grd-session-rdp.c:895-951`, `1244-1244` | 读取客户端偏好，必要时重写为 32bpp；在扩展模式下也写回桌面宽高。 |
| `FreeRDP_DesktopResize` | `src/grd-session-rdp.c:904-908` | 不支持桌面缩放的客户端会被拒绝，保证后续 monitor layout 变更可用。 |
| `FreeRDP_PointerCacheSize` | `src/grd-session-rdp.c:995-1010`, `src/grd-rdp-cursor-renderer.c:498-517` | 缺失或零值导致连接终止；成功时决定本地光标缓存槽数量，影响带宽与切换性能。 |
| `FreeRDP_FastPathOutput` | `src/grd-session-rdp.c:1000-1004` | 未开启 FastPath 的客户端被拒绝，因为 GNOME 仅实现 FastPath 渲染通道。 |
| `FreeRDP_VCFlags` / `FreeRDP_CompressionLevel` / `FreeRDP_VCChunkSize` | `src/grd-session-rdp.c:1011-1015` | 仅记录在日志中，帮助排查虚拟通道压缩策略。 |
| `FreeRDP_NetworkAutoDetect` | `src/grd-session-rdp.c:1017-1051`, `826-840`, `800-821`, `src/grd-rdp-graphics-pipeline.c:994-1010, 1448-1479` | 缺失时警告高延迟体验并禁用音频；若存在，则在握手中启动 `GrdRdpNetworkAutodetection`，供 GFX/AUDIO 调整 RTT/BW。 |
| `FreeRDP_AudioPlayback` | `src/grd-session-rdp.c:1024-1040`, `1652-1675` | 在缺少网络自适应或移动端平台上被服务端禁用；若仍为 TRUE 且不是 RemoteConsole，会初始化音频输出通道。 |
| `FreeRDP_AudioCapture` | `src/grd-session-rdp.c:1677-1680` | 驱动是否创建音频输入（麦克风）虚拟通道。 |
| `FreeRDP_RemoteConsoleAudio` | `src/grd-session-rdp.c:1671-1676` | 若客户端是 RemoteConsole 模式则跳过音频回放 DVC。 |
| `FreeRDP_MonitorCount` | `src/grd-session-rdp.c:917-961`, `src/grd-rdp-monitor-config.c:294-321`, `src/grd-rdp-renderer.c:362-380` | 判断是否使用 Client Monitor Data PDU。超过服务端允许运行的显示数量将退回 Core Data 布局。 |
| `FreeRDP_MonitorDefArray` | `src/grd-rdp-monitor-config.c:224-267`, `src/grd-rdp-renderer.c:368-379` | 读取每个 monitor 的位置与尺寸，用于构建 `GrdRdpMonitorConfig` 并在重置渲染器时填充 `MONITOR_DEF`。 |
| `FreeRDP_HasMonitorAttributes` | `src/grd-rdp-monitor-config.c:241-267` | 决定是否解析 `physicalWidth/Height`、方向与缩放因子，以便 PipeWire 初始化虚拟物理尺寸。 |
| `FreeRDP_DesktopWidth` / `FreeRDP_DesktopHeight` | `src/grd-rdp-monitor-config.c:110-145`, `src/grd-rdp-renderer.c:350-353` | 构造单虚拟显示布局时读取；渲染器也从 settings 中获取当前尺寸以触发 GFX reset。 |
| `FreeRDP_DesktopPhysicalWidth` / `FreeRDP_DesktopPhysicalHeight` | `src/grd-rdp-monitor-config.c:114-138` | 记录显示实际尺寸，供物理 DPI 推算。 |
| `FreeRDP_DesktopOrientation` | `src/grd-rdp-monitor-config.c:118-138` | 转换为内部 `GrdRdpMonitorOrientation`，同步虚拟显示方向。 |
| `FreeRDP_DesktopScaleFactor` | `src/grd-rdp-monitor-config.c:120-138` | 存储客户端建议缩放级别，最终转换为 100–500 的合法取值。 |
| `FreeRDP_GfxAVC444v2` / `FreeRDP_GfxAVC444` / `FreeRDP_GfxH264` | `src/grd-rdp-graphics-pipeline.c:218-247, 1438-1481` | 判定客户端是否支持 H264/AVC444，从而决定是否创建 NVENC 会话、是否发送 AVC420 帧或降级为 RFX Progressive。 |

## 服务端设置

| 配置键 | 设置位置 | 目的/影响 |
| --- | --- | --- |
| `FreeRDP_NtlmSamFile` | `src/grd-session-rdp.c:1191-1197` | 将 GNOME 生成的临时 SAM 路径写入，供客户端 NTLM 认证。 |
| `FreeRDP_RdpServerCertificate` / `FreeRDP_RdpServerRsaKey` | `src/grd-session-rdp.c:1204-1226` | 注入 GNOME 设置提供的证书与私钥，实现 TLS/NLA 握手。 |
| `FreeRDP_RdpSecurity` / `FreeRDP_TlsSecurity` / `FreeRDP_NlaSecurity` | `src/grd-session-rdp.c:1228-1230` | 关闭传统 RDP/TLS ，强制 NLA，满足安全基线。 |
| `FreeRDP_Username` / `FreeRDP_Password` / `FreeRDP_RdstlsSecurity` | `src/grd-session-rdp.c:1232-1237` | 仅 handover 模式设置：预先写入凭据并启用 RDSTLS。 |
| `FreeRDP_OsMajorType` / `FreeRDP_OsMinorType` | `src/grd-session-rdp.c:1239-1242` | 标记服务端自身平台为 pseudo X server。 |
| `FreeRDP_ColorDepth` | `src/grd-session-rdp.c:1244-1244`, `900-952` | 强制 32bpp，必要时在 monitor 配置阶段写回桌面宽高。 |
| `FreeRDP_GfxAVC444v2` / `FreeRDP_GfxAVC444` / `FreeRDP_GfxH264` / `FreeRDP_GfxSmallCache` / `FreeRDP_GfxThinClient` / `FreeRDP_SupportGraphicsPipeline` | `src/grd-session-rdp.c:1246-1252` | 在握手前初始化 GFX 服务端能力；其中 AVC/H264 默认为禁用，等待客户端广告后再动态开启。 |
| `FreeRDP_RemoteFxCodec` / `FreeRDP_RemoteFxImageCodec` / `FreeRDP_NSCodec` / `FreeRDP_SurfaceFrameMarkerEnabled` / `FreeRDP_FrameMarkerCommandEnabled` | `src/grd-session-rdp.c:1253-1257` | 启用传统 RFX 以及帧标记，以便在客户端缺乏 GFX H.264 时回退。 |
| `FreeRDP_PointerCacheSize` | `src/grd-session-rdp.c:1259-1259` | 预设默认缓存容量（100），在客户端握手通过后可根据其声明调整。 |
| `FreeRDP_FastPathOutput` / `FreeRDP_NetworkAutoDetect` / `FreeRDP_RefreshRect` / `FreeRDP_SupportMonitorLayoutPdu` / `FreeRDP_SupportMultitransport` | `src/grd-session-rdp.c:1261-1267` | 声明服务器端能力：启用 FastPath、网络自侦测、监视器布局；关闭 Multitransport 以保持实现简单。 |
| `FreeRDP_VCFlags` / `FreeRDP_VCChunkSize` | `src/grd-session-rdp.c:1267-1268` | 设定虚拟通道压缩能力上限，默认使用 `VCCAPS_COMPR_SC` 与 16256 chunk。 |
| `FreeRDP_HasExtendedMouseEvent` / `FreeRDP_HasHorizontalWheel` / `FreeRDP_HasRelativeMouseEvent` / `FreeRDP_HasQoeEvent` / `FreeRDP_UnicodeInput` | `src/grd-session-rdp.c:1270-1274` | 声明输入路径支持情况，允许横向滚轮与 Unicode 输入，禁用未实现的相对鼠标与 QoE。 |
| `FreeRDP_AudioCapture` / `FreeRDP_AudioPlayback` / `FreeRDP_RemoteConsoleAudio` | `src/grd-session-rdp.c:1276-1278` | 默认启用音频（录制/播放/远程控制），后续可能根据客户端能力再行关闭。 |
| `FreeRDP_DesktopWidth` / `FreeRDP_DesktopHeight` | `src/grd-session-rdp.c:949-952`, `src/grd-rdp-renderer.c:222-225` | 在扩展模式中设置初始桌面尺寸，并在 PipeWire layout 变化时更新以触发 GFX reset。 |
| `FreeRDP_MonitorDefArray` / `FreeRDP_MonitorCount` | `src/grd-rdp-layout-manager.c:350-377` | 每次布局发生变化时刷新监视器数组和数量，供下一帧 GFX/RFX 使用。 |
| `FreeRDP_GfxAVC444v2` / `FreeRDP_GfxAVC444` / `FreeRDP_GfxH264` | `src/grd-rdp-graphics-pipeline.c:2000-2017` | 在接收 RDPGFX CapsAdvertise 后根据客户端 flags 更新 settings，随后驱动编码策略（NVENC/H264/RFX）。 |

以上读写流程遵循 KISS（仅保留经过验证的协议能力）、DRY（通过集中 `rdp_settings` 管理配置）与 SOLID（将显示、图形、音频的配置解释分别封装在独立组件）原则。

