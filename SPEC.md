# HRK 系统规格

## 1. 目标与范围

HRK 面向音游内核与 Game Module 开发者。v0.1.0 验证同一 ReferenceGame 在 Headless 与 Harmony Backend 中组合运行，及相同谱面、规则版本和 Replay 的确定性。
具体范围、非目标、接口和验收均由下方功能规格定义；当前迭代从迁移后的基线重新实现 v0.1.0。

## 2. 跨功能约束

- 功能需求以 spec.md 为唯一来源；设计、任务、测试计划只引用需求。
- 使用模板的 SDD 状态与审批流程；2026-09-19 用户明确要求“完成v0.1.0迭代”，授权本版本实现及验证。
- 历史代码的实现选择不能自动升级为新需求，历史 PASS 不能作为当前验证。
- 凭据、机器签名材料和构建产物不入库。应用代码与测试须从获批规格重新建立。
- C++17 内核的 Native 技术路线必须保留；模板 ArkTS / ArkUI 仅用于 Harmony 应用外壳。

## 3. 功能索引

| Feature ID | domain | 规格 | 优先级 | 状态来源 |
|---|---|---|---|---|
| PROD-HRK | product | [rhythm-kernel](specs/product/rhythm-kernel/spec.md) | P0 | 规格 front matter |
| PROD-INPUT-RELIABILITY | product | [v0.1.1 输入可靠性](specs/product/input-reliability/spec.md) | P1，确认核心正确性缺陷后升级 P0 | TASK-001～003 完成，AC-001 及工具验证 PASS；整体状态以 front matter 为准 |

v0.1.1 调查契约仅覆盖 LI-001～LI-004，不替换已签收 v0.1.0 的行为契约；修复差异需按新规格中的调查与评审门禁定稿。

## 4. 系统接口与验收

公共接口、时间域、Replay 及依赖边界见 [功能规格](specs/product/rhythm-kernel/spec.md)。
系统验收引用该规格的 AC 与 [测试策略](TEST_PLAN.md)，不以文档治理检查替代 Headless、Harmony 或设备验证。
