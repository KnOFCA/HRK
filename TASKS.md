# HRK 任务索引

## 本次文档迁移

用户于 2026-09-19 明确要求：“按照harmony-agent-template下的模板重构HRK项目文档，移除HRK下现有代码”。授权范围为文档迁移和旧代码清理。

| 工作 | 验收依据 | 状态 |
|---|---|---|
| 保存原始需求和历史证据 | 迁移记录的快照哈希 | DONE |
| 建立系统文档、五件套和索引 | 模板结构与链接检查 | DONE |
| 移除旧代码、CI、构建配置与产物 | 迁移清单中路径不存在 | DONE |
| 检查并记录结果 | VALIDATION 实际命令 | DONE |

## v0.1.0 产品实现

2026-09-19 用户授权“完成v0.1.0迭代”。规格已审查并明确序列化、规则、输入、seek 和测量契约；实现及验证按 [功能任务](specs/product/rhythm-kernel/tasks.md) 执行。

本轮七项功能任务均 DONE；验收结果和剩余环境限制见 [验证记录](VALIDATION.md)。

## 后续优先整改：LateInput（P1，下一迭代优先）

v0.1.1 配套文档已建立：[规格](specs/product/input-reliability/spec.md)、[设计](specs/product/input-reliability/design.md)、[任务](specs/product/input-reliability/tasks.md)、[测试计划](specs/product/input-reliability/test-plan.md)、[验证记录](specs/product/input-reliability/validation.md)、[根因调查报告](specs/product/input-reliability/root-cause.md)。2026-09-20 用户仅授权文档编写，明确不编码、不测试；下列整改项仍为 TODO，未实施或验收。

用户本轮明确要求登记；不计入 v0.1.0 新增功能实现。

| 工作项 | 调查与完成条件 | 状态 |
|---|---|---|
| LI-001 原因定位 | 区分平台分发延迟、事件乱序、重复事件、音频/输入时钟映射及水位推进；保留可复现输入与时间线，给出有证据的根因 | TODO |
| LI-002 事件类型 | 对被拒绝的 DOWN/MOVE/UP/CANCEL 分别计数，记录 pointer、原始时间、映射时间、接收时间和拒绝原因；采集方式不得破坏实时路径约束 | TODO |
| LI-003 判定影响 | 对比接收前原始输入、接受输入和判定结果，确认是否改变 PERFECT/MISS、分数或 pointer 生命周期；不能仅凭已接受 Replay 一致认定无影响 | TODO |
| LI-004 修复及回归 | 先完成规格与设计评审，再实施修复；覆盖双指、密集 MOVE、暂停恢复、EOF 和不同刷新率；重跑真机与 Headless 并记录残余拒绝的边界 | TODO |

基线：两轮真人测试 LateInput=17/3、overflow=0，用户感知正常、已记录 Replay 与 Headless 一致。被拒绝事件的类型及判定影响尚未证实；如调查确认核心正确性缺陷，应升级为 P0 并按门禁处理。详见 [验证记录](VALIDATION.md)。
