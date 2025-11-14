# 任务：对比 RFX Progressive 与 SurfaceBits 推帧方案

## 上下文
- 用户关心当前项目内两条推帧路径（Rdpgfx Progressive vs SurfaceBits）在性能与画面效果上的差异，用于评估默认策略与回退路径。
- 项目基于 GLib + FreeRDP，编码层支持 Raw/RFX/Progressive，传输层包含 Rdpgfx 管线与传统 SurfaceBits。

## 计划
- [x] 步骤1：审阅 `encoding/`、`session/`、`doc/` 中与 RFX Progressive、SurfaceBits 相关的实现与文档，提取关键机制与配置。
- [x] 步骤2：整理两种推帧流程的带宽/CPU/延迟特性与失败回退条件，结合代码路径描述性能与画面特征。
- [x] 步骤3：形成结论与建议，明确在何种场景优先 Progressive、何时回退 SurfaceBits，并记录未解问题便于后续验证。

## 状态备注
- 关键步骤已完成，等待用户反馈与后续决策。
