# HRK v0.1.1 任务计划

依据 [spec](spec.md) 和 [design](design.md)。Implementation=NotStarted，Validation=NotRun，Release=NotAccepted。当前仅编写文档；下列实现、采集和测试任务均未启动。

## 1. 任务清单

路径为未来预计修改范围，实际新增文件在规格定稿时登记。测试编号定义于 [test-plan](test-plan.md)。

| 任务编号 | 来源 | 规格 | 受影响文件/交付物 | 工作与完成条件 | 依赖 | 对应测试 | 状态 |
|---|---|---|---|---|---|---|---|
| PROD-INPUT-RELIABILITY-TASK-001 | LI-001/002 | §3 S1、REQ-001 | 本功能五件套 | 定稿采集schema、容量、内存预算、注入接口，记录审查与真实实现授权 | 无 | TEST-001/002 | TODO |
| PROD-INPUT-RELIABILITY-TASK-002 | LI-002 | REQ-001 | `src/platform/harmony/backend.*`、`entry.cpp`、`src/app/composition.*`、`src/kernel/diagnostics/`、`src/kernel/gameplay/session.*` | 实现全链路采集及按phase/reason计数，满足AC-001 | TASK-001 | TEST-001/002 | TODO |
| PROD-INPUT-RELIABILITY-TASK-003 | LI-001 | REQ-002 | `src/platform/headless/*`、`tests/hrk_tests.cpp`、拟新增工具/夹具 | 实现按原调度注入、时钟查询回放、最小复现和对照 | TASK-002 | TEST-003 | TODO |
| PROD-INPUT-RELIABILITY-TASK-004 | LI-001/003 | REQ-002/003 | `root-cause.md`、未来evidence目录 | 采集真机事件，形成分类、因果链及判定/pointer差异，满足AC-002/003；未复现不标DONE | TASK-003 | TEST-003/004/007 | TODO |
| PROD-INPUT-RELIABILITY-TASK-005 | LI-004 | REQ-004、§3 S4 | 本功能规格/设计、必要ADR和系统索引 | 根据证据冻结修复契约、兼容差异与预期夹具，关闭§8相关未决项，记录授权 | TASK-004 | TEST-005 | TODO |
| PROD-INPUT-RELIABILITY-TASK-006 | LI-004 | REQ-004 | 经TASK-005确认的`input/time/session/backend`文件及测试 | 最小修复确认缺陷，AC-004通过；不做无关重构 | TASK-005 | TEST-005/006 | TODO |
| PROD-INPUT-RELIABILITY-TASK-007 | LI-004 | REQ-005 | `tests/hrk_tests.cpp`、`tests/DEVICE.md`、现有设备工具及未来夹具 | 执行主机、真机、生命周期、帧率及性能回归，AC-005/006通过 | TASK-006 | TEST-006/007/008 | TODO |
| PROD-INPUT-RELIABILITY-TASK-008 | LI-001～004 | §3 S6、§7 | `validation.md`、未来manifest、TASKS/VALIDATION/CHANGELOG | 归档完整证据和稳定版本引用，逐条关闭AC及LI；真实签收后更新发布维度 | TASK-007 | 全部 | TODO |

## 2. 依赖与关闭规则

TASK-001 → 002 → 003 → 004 → 005 → 006 → 007 → 008。调查期间可完善文档，但不能越过S4实施未定义的行为修复。

LI-001由AC-002关闭；LI-002由AC-001关闭；LI-003由AC-003关闭；LI-004由AC-004～006及归档关闭。文档完成不关闭任何LI工作项。

## 3. 风险与范围

真机不可用、未捕获原因、轨迹丢失、诊断扰动、版本行为冲突均需保留真实状态；处理要求引用spec §3/5/8。当前没有执行失败或环境阻塞证据，不预填BLOCKED。

不包含校准、Hold或其他路线功能；本轮文档工作不创建源代码、测试代码、测试夹具和验证manifest。
