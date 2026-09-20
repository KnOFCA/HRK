# HRK SDD 文档迁移（2026-09-19）

## 授权与范围

本次用户原话：“按照harmony-agent-template下的模板重构HRK项目文档，移除HRK下现有代码”。执行者据此迁移和清理，不代签产品规格审批。
源为同级 harmony-agent-template，未修改模板仓库；HRK 的 .git 保留。迁移前 Git 状态显示现有项目文件均未跟踪，因此专门保存文档快照。

## 文件及条款映射

| 原位置 / 内容 | 新位置 / 处理 |
|---|---|
| require_describe/v0.1.0 规格，除下列章节外 | specs/product/rhythm-kernel/spec.md，保留原章节与正文 |
| 原规格 §4、23、24、29、30、39 | 功能 design.md |
| 原规格 §37 | 功能 tasks.md 的计划参考 |
| 原规格 §40 | docs/ROADMAP.md |
| 原测试与验收说明全文、全部 TC 编号 | 功能 acceptance.md，作为 spec 的规范附件；test-plan.md 引用 |
| 原 README、docs、需求原件和证据 | docs/archive/pre-reset，逐文件哈希保存，不继承验收 |
| docs/ARCHITECTURE.md、OPEN_SOURCE.md 等旧入口 | 移除，历史副本在 archive；当前入口由系统文档统一导航 |
| app、core（含第三方、测试、资源） | 移除 |
| 旧 tools 下 5 个 Python 脚本 | 移除；替换为模板 tools/sdd、validation、harmony |
| .github、.clang-format、CMakeLists.txt | 移除 |
| out、.hvigor | 移除旧构建产物与缓存 |
| src、tests | 只保留说明，无 HRK 产品代码 |

原项目无 Feature ID，故 legacy_ids 为空。新增 PROD-HRK 为整个 v0.1.0 架构原型，REQ / AC / TASK / TEST 001—007 对应架构、时间、音频输入、玩法运行时、Replay、Harmony、质量发布七组。原 TC 编号保留，不重编号或遗漏；未来拆分功能需另记映射。

## 验证与局限

检查条件见 [TEST_PLAN](../../TEST_PLAN.md)，实际结果见 [VALIDATION](../../VALIDATION.md)。
保留原规范的全部章节，但建议和示例尚待下一轮确定为可执行契约；产品维持 Draft / Pending，不宣称已通过完整规格审查。
历史证据限制见 [archive](../archive/README.md)。
