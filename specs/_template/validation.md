# <功能名称> —— 验证证据

> | 项目 | 值 |
> |---|---|
> | Feature ID | `<DOMAIN-FEATURE>` |
> | 本文件职责 | 记录**实际执行**的命令、输出与逐条 AC 结论（Evidence） |
> | 总览索引 | [`../../../VALIDATION.md`](../../VALIDATION.md) |
> | Evidence manifest | `evidence/<DOMAIN-FEATURE>/<run-id>/manifest.json` |
> | 最近更新 | YYYY-MM-DD |
>
> 本文件**不重新定义需求**。判定阈值见 `spec.md`，验证方式见 `test-plan.md`。

---

## 1. 验证记录索引

| Run ID | 日期 | 范围 | 结论 | Evidence |
|---|---|---|---|---|
| `<DOMAIN-FEATURE>-YYYYMMDD-01` | | | `PASS` / `FAIL` / `BLOCKED` | `evidence/<DOMAIN-FEATURE>/<run-id>/` |

---

## 2. Run `<DOMAIN-FEATURE>-YYYYMMDD-01`

### 2.1 环境

| 项目 | 值 |
|---|---|
| 操作系统 / 运行时 | |
| 验证器版本 | |
| Specification revision | |
| Design revision | |
| Implementation commit | |
| External builder / tool version | |

### 2.2 执行结果

```text
Build:            PASS / FAIL / BLOCKED
Static Analysis:  PASS / FAIL / BLOCKED
Unit Tests:       <通过数> / <总数>
Integration Tests:<通过数> / <总数>
Regression Tests: <通过数> / <总数>
Acceptance:       <通过数> / <总数>
```

### 2.3 验收标准逐条结论

| 验收标准 | 结论 | 证据（命令 / 输出摘要） | 日期 |
|---|---|---|---|
| `<DOMAIN-FEATURE>-AC-001` | | | |

### 2.4 失败分类

A 实现缺陷 / B 规格不完整 / C 规格错误 / D 环境或工具问题

### 2.5 结论

`PASS` / `FAIL` / `BLOCKED`

---

## 3. 未通过项与处理

| 编号 | 失败项 | 失败分类 | 处理动作 | 状态 |
|---|---|---|---|---|
| | | | | |

---

## 4. 已知限制与剩余问题

| 编号 | 内容 | 影响 | 后续计划 |
|---|---|---|---|
| | | | |
