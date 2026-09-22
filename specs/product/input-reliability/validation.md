# HRK v0.1.1 验证记录

对应 [规格](spec.md) 与 [测试计划](test-plan.md)。

## 当前状态

2026-09-21：TASK-001～003 的调查契约、采集和离线工具完成；TASK-002/AC-001 与 TASK-003 工具测试 PASS。整体 Implementation=InProgress，真人根因及剩余验收未完成，Release=NotAccepted；未将部分验收写为整轮 PASS。

2026-09-20 的文档草案按当时授权未进行编码和测试。2026-09-21 用户要求完成 TASK-001，本次完成调查契约、设计和测试映射审查，并运行文档治理检查；没有进行产品构建、输入采集、主机产品测试、真机或性能验证。文档检查不构成功能 AC 验收。

## TASK-001 审查与文档验证

授权证据：当前会话用户“完成`v0.1.1` task001”；范围解释见 spec §1。审查者为执行 Agent，不代签产品负责人；审查覆盖当前源码基线 `06df6030df54a2b81d8e26d7f33148d880280f61`，未修改产品源代码。

| 审查项 | 依据与结果 |
|---|---|
| 清晰性与数据契约 | PASS：spec §4.4/4.5 定义严格字段形状、数值/空值、枚举、关联和唯一终态；正文为唯一事实源 |
| 资源与失败路径 | PASS（文档审查）：spec §4.6 定义容量预算、满/分配失败、冻结超时、导出失败重试；实际容量及热路径性能留给实现测试 |
| 并发与生命周期 | PASS（文档审查）：按实际 touched/frame/control 关系明确槽提交、在途写者与冻结所有权，不将 SPSC 当多写者队列；不支持的并发明确不完整 |
| 重现完整性 | PASS（文档审查）：spec §4.7 覆盖公共 position/clockSample 查询、poll 可见顺序、控制清理、pending 原映射及歧义拒绝 |
| 接口、错误与测试映射 | PASS（文档审查）：spec §4.8 定义未来 CLI，TEST-001/002/003 增补边界；TEST-009 对应 S1 文档门禁 |
| 兼容性、范围与授权 | PASS（文档审查）：保留原队列容量、水位/判定/Replay；未批准 S4 策略，TASK-002～008 未启动 |

治理检查命令、退出码、日志和文档快照登记在 `evidence/PROD-INPUT-RELIABILITY/2026-09-21-task001/manifest.json`。该目录仅证明 S1 文档检查，不能作为任何 AC 的产品运行证据。

执行环境：Windows PowerShell、Git for Windows Bash、Node.js v24.18.0。实际命令为 `& 'C:/Program Files/Git/bin/bash.exe' -lc 'bash tools/sdd/check.sh'`，最终退出码 0：SDD lint、Markdown 链接、证据来源检查、22/22 验证器自测、43/43 必需输入和三领域模板演练全部 PASS，FAIL=0、BLOCKED=0。`git diff --check` 退出码 0。

过程记录：直接启动 Bash 的两次尝试因未加载 Git Bash 环境、找不到 dirname 退出 1；改为 login shell 后恢复。首次完整检查发现 Approved 必须匹配 Approved 的 spec_status，且需明确 Workflow Step 定义；修正规格后全量检查通过。未修改或放宽验证器。归档保留失败与成功日志，不把已解决问题记为当前 BLOCKED。

## 验收进度

| 验收标准 | 状态 | 说明 |
|---|---|---|
| PROD-INPUT-RELIABILITY-AC-001 | PASS | TASK-002：四 phase、终态对账、队列/诊断满、缺关联与导出失败；主机测试及 Native 构建，证据见下文 |
| PROD-INPUT-RELIABILITY-AC-002 | NotRun | TASK-003 合成复现工具 PASS；真人采集与根因确认尚未执行，不能关闭 LI-001 |
| PROD-INPUT-RELIABILITY-AC-003 | NotRun | 被拒绝事件影响未知 |
| PROD-INPUT-RELIABILITY-AC-004 | NotRun | 修复策略尚待调查与评审 |
| PROD-INPUT-RELIABILITY-AC-005 | NotRun | 未执行Headless或Replay回归 |
| PROD-INPUT-RELIABILITY-AC-006 | NotRun | 未执行真机与性能回归 |

## 后续证据登记

真实执行后按spec §7登记运行环境、实际命令/退出码、版本引用、哈希、逐AC结果及失败分类。记录历史结果时必须标记来源，不将v0.1.0的92项测试或真人17/3次LateInput写成本轮结果。

根因与影响见 [调查报告](root-cause.md)，当前状态为待调查。后续设备调查与修复尚未执行，不因尚未开始而虚构环境 BLOCKED。

## TASK-002 实现与验证（2026-09-21）

授权：当前会话用户“完成`v0.1.1` task002”，随后要求继续完成项目。范围为 REQ-001 采集、TEST-001/002 的采集部分及回归；S4 行为修复未获批准。执行 Agent 审查了新增既有命令观测枚举和空 poll 边界，requirements_version=3；未修改游戏输入策略。

结果：**PASS（TASK-002/AC-001）**。实际实现为固定容量 InputTrace、内部 envelope、Session 分支/时钟观测、Harmony 回调与控制接入、冻结和离线导出。说明见 [design](design.md) §7；使用入口见 [INPUT_TRACE](../../../tests/INPUT_TRACE.md)。

| 验证 | 实际结果 |
|---|---|
| `cmake --build build/host --config Debug` | PASS，MSVC /W4 /WX，最终编译无 warning |
| `cmake --build build/harmony-native` | PASS，Harmony arm64-v8a Native 库链接通过；Clang -Werror，无源码 warning。CMake 打印既有 OHOS 平台识别提示，不影响构建 |
| `ctest --test-dir build/host -C Debug --output-on-failure` | PASS，4/4：既有核心、输入轨迹、架构和固定 Headless Replay |
| TEST-001 | PASS：四 phase 的接受/拒绝/清空/pending，跨 update 保留首次消费和映射、epoch、pointer 边界、公共时钟调用与失败 |
| TEST-002 采集范围 | PASS：专用 C++ 测试共 27 项；覆盖容量、失败重试、并发存储、冻结超时/提交/销毁、溢出及计数、缺关联、EOF/ReplayFull、诊断开关与游戏结果一致 |
| JSONL 导出独立检查 | PASS：16 个文件的精确字段、64 位字符串、null、原始字段一致性、action/query 顺序、sample/control 关联及 summary 对账 |
| 内存和热路径 | PASS：记录槽 240 bytes；该运行采集对象加实际分配为 31,525,232 bytes，begin 实测分配 31,523,168 bytes；加两个队列关联等仍低于 33 MiB。受测 submit/poll/update/pause 的新增堆分配为 0 |

证据目录：`evidence/PROD-INPUT-RELIABILITY/2026-09-21-task002/`，manifest 登记真实命令/退出码、日志、规格/源码/验证器哈希、合成 Chart/WAV 与代表性轨迹。当前实现没有创建 Git 提交；稳定 `refs/tags/v0.1.0` 加精确字节的 workspace-delta 用于重建被验证版本，不把旧 tag 冒充实现版本。容量压力轨迹在 build 保留，通过测试日志及源代码可重新生成。

已解决的过程问题：首次 MSVC 构建被沙箱阻止读取 Windows SDK，使用已授权的沙箱外构建后恢复；严格编译发现测试局部变量遮蔽以及 getenv 的 MSVC 警告，均已修正。首次独立导出检查发现空 poll 消耗 action 序号，已在实际转交时才分配；没有放宽检查。保留最终测试日志和可用的失败构建日志。

范围限制：TEST-002 的外部文件解析拒绝、TASK-003 CLI/RecordedAudioBackend 尚未实现；本轮未执行真机、HAP 安装、Linux sanitizer 或设备性能测量，这些后续任务不标 PASS。AC-002～006 仍 NotRun，不把合成 fixture 的 complete=true 当成真人根因证据。LI-002 按 AC-001 关闭采集能力；真人原因及影响继续留在 TASK-004。

文档门禁：`& 'C:/Program Files/Git/bin/bash.exe' -lc 'bash tools/sdd/check.sh'` 退出 0；SDD lint、Markdown 链接、provenance、22/22 验证器测试、43/43 必需输入、三领域模板演练均 PASS，FAIL=0、BLOCKED=0。

收尾记录：一次测试启动早于主机构建完全结束，Windows 链接器因可执行文件占用报 LNK1168；等待测试进程结束后顺序重建成功，再执行最终回归。该已解决失败日志单独保留，不作为最终构建结果。

## TASK-003 实现与验证（2026-09-21）

授权：用户要求“完成`v0.1.1` task003并提交已完成的工作”。执行 Agent 对照 §4.3/4.7/4.8 审查后实现调查工具，未修改水位、输入提交策略、命中窗口或 Replay 格式；S4 未通过，未实施行为修复。

结果：**PASS（TASK-003，TEST-003 与 TEST-002 外部解析范围）**。

| 验证 | 实际结果 |
|---|---|
| `cmake --build build/host --config Debug` | PASS，MSVC /W4 /WX |
| `ctest --test-dir build/host -C Debug --output-on-failure` | PASS，5/5：核心、采集、CLI、架构、固定 Headless Replay |
| 采集夹具生成 | 28/28 C++ 测试；独立检查 26 个 JSONL，新增五组 original/control 合成轨迹 |
| `hrk_trace_cli` | 100 项检查 PASS：严格解析、完整文件重放、查询/调度拒绝、哈希、差异、最小化及 I/O/不覆盖失败路径 |
| Clang 19 严格语法检查 | PASS，使用 VS 2022 的 MSVC 14.44 头文件；命令见 manifest |
| 最小化与对照 | PASS：批内逆序 event 2 的 before_watermark 拒绝可重放；删除无关 status action 后保留原事件号、依赖和目标分支；派生夹具可再次重放 |

证据目录为 `evidence/PROD-INPUT-RELIABILITY/2026-09-21-task003/`。manifest 保存实际命令、源码/规格/设计/验证器哈希、CTest 日志、五组合成轨迹/完整结果/差异和最小夹具。以稳定 `refs/tags/v0.1.0` 加字节级 workspace-delta 重建验证源；该旧 tag 仅作重建基线，不冒充新实现版本。本次提交同时包含先前 TASK-001/002 已完成且尚未提交的工作。

过程问题：沙箱一度阻止 MSBuild 读取 Windows SDK，沙箱外构建恢复；额外 Clang 检查默认误选 VS 18 新版 STL，显式选择与 Clang 19 匹配的 VS 2022 头文件后通过。最初队列重放未纳入队列满对前驱可见性的约束，现已修正并由 queues.jsonl 回归；未放宽校验。最小化测试补充了可删除的无关查询，以验证实际删减，而非仅输出原文件。

限制：工具按已记录的队列可见性建立等价调度，不能恢复没有记录的跨线程物理时序；约束无法满足时报 TraceScheduleAmbiguous。合成对照仅证明当前机制与工具，不将其归因为历史真人 17/3 次 LateInput。TASK-004～008、AC-002～006、真机、Linux sanitizer 和设备性能仍未验收。

文档治理：完整 `bash tools/sdd/check.sh` PASS（22/22 验证器自测、43/43 必需输入、三领域模板演练，FAIL=0、BLOCKED=0），`git diff --check` PASS。

## 2026-09-22 CI 失败调查

用户进一步授权检查并修复 GitHub Actions。远端最新失败为 [run 35516590462](https://github.com/KnOFCA/HRK/actions/runs/35516590462)，对应规划提交 `06df6030df54a2b81d8e26d7f33148d880280f61`；其构建和 sanitizer 测试成功，SDD 步骤失败，尚未包含本地 TASK-001～003 的实现。

从该 commit 的 Git archive 在隔离目录执行原 lint，唯一失败为 `C9-terminology`：规格未定义 `Workflow Step`。TASK-001 已补齐该定义，本次将它与已完成工作正式提交；不放宽验证器。新证据使用稳定 tag，CI checkout 同时改为获取完整历史与 tags，以满足既有 provenance 门禁。GitHub 作业原始日志 API 返回 403，因此具体 C9 根因来自精确提交的本地复现，不能声称已下载远端完整日志。
