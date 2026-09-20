# <功能名称> —— 任务计划

> | 项目 | 值 |
> |---|---|
> | Feature ID | `<DOMAIN-FEATURE>` |
> | 对应规格 | [`spec.md`](spec.md) v0.1.0（`Draft`） |
> | 对应设计 | [`design.md`](design.md) |
> | 本轮迭代 | TBD |
> | Implementation | `NotStarted` |
> | Validation | `NotRun` |
> | Release | `NotAccepted` |
>
> 任务必须来源于规格。规格 `Approval: Pending` 时不得声称已获批准；任务完成不等于发布。

---

## 1. 任务清单

每个任务至少标明：对应规格章节、受影响文件、实现内容、依赖关系、对应测试、完成条件。

| 任务编号 | Legacy | 规格 | 受影响文件 | 实现内容 | 依赖 | 对应测试 | 完成条件 | 状态 |
|---|---|---|---|---|---|---|---|---|
| `<DOMAIN-FEATURE>-TASK-001` | | `<DOMAIN-FEATURE>-REQ-001` | `src/...` | | 无 | `tests/...` | | `TODO` |

状态取值：`TODO` / `DOING` / `DONE` / `BLOCKED`。
`DONE` 只能在满足 `CONTRIBUTING.md` §5 完成门禁后使用。

## 2. 执行顺序与依赖

```text
<DOMAIN-FEATURE>-TASK-001 数据模型
  ↓
核心逻辑
  ↓
接口
  ↓
集成测试
```

| 顺序 | 任务 | 前置任务 | 可并行 |
|---|---|---|---|
| 1 | `<DOMAIN-FEATURE>-TASK-001` | 无 | 否 |

## 3. 规格 ↔ 实现 ↔ 测试映射

| Canonical requirement | Legacy | 任务 | 实现文件 | 测试文件 | 验收标准 |
|---|---|---|---|---|---|
| `<DOMAIN-FEATURE>-REQ-001` | | `<DOMAIN-FEATURE>-TASK-001` | `src/...` | `tests/...` | `<DOMAIN-FEATURE>-AC-001` |

## 4. 本轮不做的内容

TBD —— 明确列出本任务范围之外的事项，避免"顺便实现"（`agent.md` §9.1、§9.5）。

| 编号 | 事项 | 原因 |
|---|---|---|
| TBD | | |

## 5. 风险与阻塞

| 编号 | 风险 / 阻塞 | 影响任务 | 应对 | 状态 |
|---|---|---|---|---|
| TBD | | | | |

## 6. 完成检查

- [ ] 规格已更新并通过结构审查
- [ ] 实现与规格一致
- [ ] 所需测试已存在
- [ ] 所有必须测试通过
- [ ] 所有验收标准通过
- [ ] 不存在已知 P0 问题
- [ ] 回归测试通过
- [ ] 文档保持一致
- [ ] 未引入未记录的新行为
- [ ] 规格已获真实负责人批准（`Approval: Approved`）—— 未批准时 `Release: NotAccepted`
