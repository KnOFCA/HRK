# 规格组织与治理

## 1. 领域

`product` 定义产品行为，`engineering` 定义工程能力，`process` 定义协作与交付流程。目录为 `specs/<domain>/<feature>/`；`_template` 不是功能实例。

## 2. 五件套

| 文件 | 唯一职责 |
|---|---|
| spec.md | 需求、契约、约束、异常、AC 及审批 |
| design.md | 实现方案与 ADR 引用 |
| tasks.md | 需求到实现任务的映射 |
| test-plan.md | AC 到测试与执行方法的映射 |
| validation.md | 实际命令、结果、版本与证据 |

## 3. 唯一事实来源

系统 SPEC > 功能 spec > design > tasks / test-plan > 工程手册 > validation > README > 推断。
字段、接口、错误码、门禁与验收标准只在规格中定义；其他文档引用。冲突先修正规格，不通过降低测试要求掩盖问题。

## 4. 生命周期与审批

| 字段 | 允许值 |
|---|---|
| spec_status | Draft / Ready-for-Review / Reviewed / Approved / Superseded |
| approval | Pending / Approved |
| implementation_status | NotStarted / InProgress / Complete |
| validation_status | NotRun / PASS / FAIL / BLOCKED |
| release_status | NotAccepted / Accepted / Released / Superseded |

Reviewed 表示审查通过；Approved 需要真实的 approver、approval_date、approval_evidence。已有明确授权可以记录为证据，不代签、不重复索取同一授权。Accepted / Released 要求实际验收 PASS。

## 5. ID

Feature 使用 `PROD-<NAME>` / `ENG-<NAME>` / `PROC-<NAME>`。条款使用 `<Feature>-REQ-001`、`<Feature>-AC-001`、`<Feature>-TASK-001`、`<Feature>-TEST-001`。
编号全仓库唯一、删除后不复用。定义位于对应文件表格首列；其他位置只引用。legacy_ids 默认为空；迁移时登记 [映射](../docs/migration/SDD_ARCHITECTURE_MIGRATION.md)。

## 6. Metadata

使用 [规格模板](_template/spec.md) 中的完整 front matter。canonical_spec 等于自身仓库相对路径，domain 等于所属目录，requirements_version 随需求变更递增。解析器仅支持标量和单行列表，不支持完整 YAML。
可选 required_fields / additional_fields / total_fields 仅检查数字求和，不代表任何预设业务字段数量。

## 7. 质量门禁

清晰、完整、可验证。审查正常、异常、边界、失败、安全、性能、兼容性与可测试性。P0 必须满足，P1 重要，P2 可选。

## 8. 新建功能

运行 `node tools/sdd/new-feature.mjs <domain> <feature-name> <FEATURE-ID>`。
工具复制五件套、填入标识并修正目录深度；仍需人工填写所有 TBD 和验收条件。
更新 SPEC、TASKS 的索引，完善设计和测试计划，记录实际授权，然后实施、验证与归档。空白模板检查通过不意味着需求完整或已经批准。

## 9. 工作流概念

- Workflow Step：按顺序执行的单元。
- Validation Gate：步骤中客观可判定的通过条件，失败时禁止后续相关动作。
- Action Step：产生或修改交付物的动作。
- Recording Step：只记录证据的动作。

编号和门禁由每个功能自行定义，不继承旧项目的步骤数量。

## 10. 功能索引

已签收功能为 [HRK v0.1.0](product/rhythm-kernel/spec.md)；下一轮 [v0.1.1 输入可靠性](product/input-reliability/spec.md) 已完成 TASK-001～003、AC-001 和离线工具验证；整体实现仍 InProgress，真人根因及后续验收尚未完成。系统索引见 [SPEC](../SPEC.md)。
