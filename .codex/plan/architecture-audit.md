# doc/architecture.md 校验任务

## 背景
- 用户要求基于当前源码校验 `doc/architecture.md` 的准确性，纠正错误描述并补足缺失内容，同时指出仍需优化的部分。
- 需遵守仓库开发约定（中文说明、SOLID/KISS/DRY/YAGNI 等），并在 `doc/changelog.md` 记录修改。

## 开发计划
1. [x] 通读 `doc/architecture.md`，列出需要核实的模块/数据流（核心层、capture、encoding、session、rdpgfx 等）。
2. [x] 对照源码（尤其是 `src/core`, `src/capture`, `src/encoding`, `src/session`, `src/transport`, `src/security`）验证描述，记录不一致或缺失信息。
3. [x] 更新 `doc/architecture.md`（必要时补充 UML）、同步 `doc/changelog.md`，并总结后续优化建议。

## 进度记录
- 当前进度：全部步骤已完成，等待用户确认。
- 风险：文档篇幅较大，需分段对照以避免漏检；若源码近期变更较多，可能需要额外时间核对。
