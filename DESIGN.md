# HRK 系统设计

需求来源：[系统规格](SPEC.md) 与 [产品规格](specs/product/rhythm-kernel/spec.md)。

## 架构

采用 C++17 Kernel、Game API、Platform API 和 Render API；ReferenceGame 通过接口接入，Headless / Harmony 提供平台实现，Composition Root 完成组装。
Harmony 应用外壳采用 Stage / ArkTS / ArkUI，Native 层承载实时运行。方案细节见 [功能设计](specs/product/rhythm-kernel/design.md)。

## 目录职责

| 路径 | 职责 |
|---|---|
| specs/product/rhythm-kernel | v0.1.0 规格五件套及验收契约 |
| specs/product/input-reliability | v0.1.1 输入可靠性五件套及根因报告框架；[设计草案](specs/product/input-reliability/design.md)，未实施 |
| specs/engineering、specs/process | 未来工程与流程功能入口，当前无实例 |
| src | kernel、interface、games、platform、app 的 C++ / ArkTS 实现 |
| tests | 按 AC 建立的主机自动化测试、固定夹具与设备步骤 |
| tools | 模板治理和构建辅助工具 |
| evidence | 新运行证据与内容哈希；设备验收单独记录 |
| docs/archive/pre-reset | 旧文档及历史证据，不作为当前事实源 |
| build | 忽略入库的临时检查及构建目录 |

工具链见 [TOOLCHAIN](docs/TOOLCHAIN.md)，决策入口见 [ADR](docs/adr/README.md)。
