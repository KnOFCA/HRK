---
id: PROD-INPUT-RELIABILITY
legacy_ids: []
title: HRK v0.1.1 输入可靠性与 LateInput 整改
domain: product
spec_status: Draft
approval: Pending
approver: null
approval_date: null
approval_evidence: null
implementation_status: NotStarted
validation_status: NotRun
release_status: NotAccepted
canonical_spec: specs/product/input-reliability/spec.md
requirements_version: 1
---

# HRK v0.1.1 输入可靠性规格

本文件是本功能需求、数据契约及验收条件的唯一事实来源。遵循 [系统规格](../../../SPEC.md) 和 [规格治理](../../README.md)。

## 1. 目标、范围与授权

完成 [LI-001～LI-004](../../../TASKS.md) 对应的原因定位、事件分类、判定影响评估、修复和真机/Headless 回归，使每个被拒绝事件能够被解释，并能从原始输入轨迹重现问题。

2026-09-20 用户仅授权编写本迭代配套文档，明确“不进行编码和测试工作”。本次交付是规划草案，不代表批准具体修复策略或启动实现。未运行构建、测试、设备采集及治理检查，不创建实际运行证据。

历史事实：v0.1.0 真人两轮诊断记录 LateInput=17/3、overflow=0，已接受事件的 Replay 与 Headless 一致；被拒绝事件类型、根因及其判定影响仍未知。出处为 [签收记录](../../../docs/acceptance/v0.1.0.md) 与 [历史验证](../../../VALIDATION.md)，这些记录不算本迭代验证结果。

本迭代不包含校准 UI、音频偏移设置、Mania/Hold、新判定规则、3D 渲染、第三方游戏迁移或对外发布。保留 C++17、平台接口隔离和 ReferenceGame 规则。

## 2. 需求

| 需求编号 | 内容 | 优先级 | 验收 |
|---|---|---|---|
| PROD-INPUT-RELIABILITY-REQ-001 | 从平台接收至 Session 消费建立可关联的输入、时钟、控制操作与水位轨迹，提供按事件阶段和拒绝原因的统计；采集失败可见 | P1 | PROD-INPUT-RELIABILITY-AC-001 |
| PROD-INPUT-RELIABILITY-REQ-002 | 调查平台分发延迟、跨触点乱序、重复、映射变化和水位推进；用轨迹和对照实验说明已证实原因及未解释部分 | P1 | PROD-INPUT-RELIABILITY-AC-002 |
| PROD-INPUT-RELIABILITY-REQ-003 | 对照原始输入、接受输入、判定与触点生命周期，量化拒绝事件对判定、分数和状态的影响 | P1 | PROD-INPUT-RELIABILITY-AC-003 |
| PROD-INPUT-RELIABILITY-REQ-004 | 依据调查修复已证实缺陷，显式定义输入提交策略及异常行为；保留原始时间戳与既有判定窗口，不通过改为处理时间、扩大命中窗口或隐藏拒绝来通过验收 | P1；确认核心正确性缺陷后升 P0 | PROD-INPUT-RELIABILITY-AC-004 |
| PROD-INPUT-RELIABILITY-REQ-005 | 在 Headless、真机及既有 Replay 上验证确定性、生命周期和实时路径；归档可追溯证据 | P1 | PROD-INPUT-RELIABILITY-AC-005、PROD-INPUT-RELIABILITY-AC-006 |

## 3. 工作流与门禁

| 步骤 | 类型 | 产出与门禁 |
|---|---|---|
| S1 调查规格定稿 | 规格审查 | 明确采集容量、接口和回放调度格式，记录真实实现授权后才可编写诊断代码；当前未通过 |
| S2 采集与复现 | Action Step | 实现采集/注入能力，保存完整运行与最小复现；区分真人、自动注入和合成夹具 |
| S3 根因与影响分析 | Action Step + Validation Gate | 按 AC-002/003 完成报告；未解释影响不得归为无害；核心正确性缺陷升 P0 |
| S4 修复规格与设计评审 | Validation Gate | 将 §8 未决策略补成唯一、可验证的契约，更新 requirements_version、设计和测试映射，记录真实授权；未通过不得实施行为修复 |
| S5 实现与回归 | Action Step + Validation Gate | 执行获批修复及 AC-004～006；已知 P0 或必要用例 FAIL/BLOCKED 均阻止验收 |
| S6 证据归档与签收 | Recording Step | 保存真实版本、运行和哈希；全部必要 AC PASS 后才可申请签收，不以报告写完替代验收 |

## 4. 输入与输出契约（草案）

### 4.1 输入及产物

输入是原始 Chart 字节、音频资源、原始平台事件、音频采样结果、Session 控制操作和 update 调度。Chart、音频、原始事件的既有合法范围引用 [v0.1.0 规格的实现前审查](../rhythm-kernel/spec.md)。

输出为独立诊断轨迹、统计摘要、最小复现、根因/影响报告、接受事件 Replay、完整判定结果与验证 manifest。诊断轨迹不混入游戏 Replay；Replay 只能证明接受事件的重放，不能替代原始轨迹。

轨迹建议采用离线导出的 UTF-8 JSON Lines，格式版本为 1。64 位整数使用十进制字符串，防止跨 JavaScript/Python 工具丢失纳秒精度。缺失或尚未产生的值写 null，不用 0 伪装采样成功。不同记录类型通过 kind 区分；字段定义如下，具体序列化接口须在 S1 定稿。

### 4.2 轨迹字段

| 字段 | 类型与适用记录 | 语义 |
|---|---|---|
| traceVersion | 整数；header | 格式版本 |
| runId | 非空字符串；header | 一次运行的关联标识，不作排序依据 |
| provenance | 对象；header | 源码/规格/策略版本、构建模式、设备型号/系统、实际刷新率、Chart/音频哈希及来源类别（真人/自动注入/合成） |
| captureConfig | 对象；header | 启用范围、预分配容量、时钟单位/时间域、输入分发余量和已定稿策略参数 |
| kind | 字符串；所有记录 | header / input / clock / control / update / summary |
| recordSequence | 无符号 64 位字符串；所有记录 | 导出中的记录序号；不冒充跨线程物理发生顺序 |
| epoch | 无符号 64 位字符串；运行记录 | 区分 start/resume/seek 建立的映射区间 |
| eventSequence | 无符号 64 位字符串；input | 平台入口分配的事件关联号，贯穿接收、入队、消费与拒绝；重复内容仍分配不同号 |
| batchSequence、pointIndex | 无符号整数；input，可空 | 同次平台回调及触点数组位置，用于恢复批内顺序 |
| pointerId、phase、position | 既有原始事件类型；input | 未改写的 ID、DOWN/MOVE/UP/CANCEL 和归一化坐标；不在采集器中去重 |
| rawHostTime、receiveHostTime、consumeHostTime | 有符号 64 位字符串；input，消费前末项可空 | 原始事件时间、回调接收时间及 Session 消费时间，均注明单调时钟域 |
| mappedSongTime | 有符号 64 位字符串或 null；input | 实际映射结果；失败时不得伪造有效时间 |
| mappingSampleSequence | 无符号 64 位字符串或 null；input | 实际使用的 clock 记录关联号 |
| watermarkBefore、watermarkAfter、songNow | 有符号 64 位字符串或 null；input/update | 实际处理水位和音频驱动的当前歌曲时间 |
| stage、disposition、error | 字符串；input | 处理阶段、接受/暂存/拒绝/清空的终态及既有错误名称；待处理事件不得统计为已接受 |
| reason | 字符串或 null；input | 可观测分支原因，例如早于 epoch、早于提交水位、队列满、非法输入或生命周期清空；根因推断另记报告 |
| sampleHostTime、sampleSongTime、sampleSuccess | 两个可空 64 位字符串及布尔值；clock | 原样保留实际音频查询及失败；失败不沿用为新成功采样 |
| operation、hostTime、targetSongTime、result | 字符串、64 位字符串、可空 64 位字符串、字符串；control | load/start/pause/resume/seek/stop/后台/Surface 操作、发生时间、seek 目标及结果 |
| updateSequence、hostTime | 无符号/有符号 64 位字符串；update | update 调用及顺序，配合 clock 记录恢复调度 |
| countsByPhaseAndReason、captureDropped、pendingAtEnd、complete | 对象、无符号 64 位字符串、无符号 64 位字符串、布尔值；summary | 分类计数、诊断丢失量、未完成事件量及采集完整性 |

每个已捕获事件必须有唯一终态或显式 pending；总入口事件量与接受、拒绝、清空、pending 数量可对账。平台队列丢失、Session 队列丢失和诊断缓冲丢失分别报告，不合并为 LateInput。完整轨迹需覆盖关联 clock/control/update；缺任何必要关联或 captureDropped 非零时 complete=false，不可据此声称根因排他性或输入零丢失。

### 4.3 离线重现与差异输出

完整复现先按接收顺序、回调批次和 update 调度重建到达行为，并注入记录的时钟成功/失败结果；不得先排序原始轨迹从而消除待查问题。原始时间戳、采样值和 epoch 均保留。派生最小复现需记录来源事件号及删减步骤。

对照路径可使用按事件时间稳定排序的合成理想输入，但必须标注其假设及适用边界，不宣称等于设备上未发生的真实操作。输出包含拒绝事件号与原因、判定完整有序序列、Timing Error、分数，以及每个控制边界和运行结束时的 active pointers。报告给出首个差异位置；无法建立可靠预期时标记未解决。

空轨迹应能够重现纯超时；非法版本、缺必要记录、非法数值和损坏关联应拒绝“完整复现”，输出具体原因，不静默补齐。采集/导出失败不能改变游戏判定；离线工具接口与专用错误码尚未定稿，不冒用既有 ReplayInvalid 表示所有轨迹问题。

## 5. 兼容性、资源与异常

- 现阶段不修改 [v0.1.0 输入水位契约](../rhythm-kernel/spec.md)。本草案不是另一套已生效行为；S4 应明确列出被替换条款、适用版本和旧行为回归范围。
- 保留 ReferenceGame 命中窗口、计分、Chart 原始字节哈希与旧 Replay 格式；旧固定 Replay 的完整结果必须保持一致。若最终策略必须改变版本语义，应在 S4 明确 kernelVersion/兼容范围，不能静默改变。
- 保留原有输入队列容量和满时策略，除非 S4 有明确需求变更。采集缓冲独立预分配，容量及内存预算在 S1 定稿；热路径不新增堆分配、文件 IO、排序导出、阻塞锁或日志洪泛。导出放在暂停/停止后的控制路径，不引入第三方运行时依赖。
- 设备性能使用 v0.1.0 规格的测量口径与阈值；分别测关闭/开启诊断的构建，并记录采集丢失。启用诊断也应满足该基线，不能以只测关闭状态证明采集无扰动。
- 缓冲满时记录丢失并标记不完整，不覆盖旧记录后宣称完整；磁盘不可写时报告导出失败，可在控制路径重试。证据不包含签名、凭据或设备个人数据。
- 覆盖时间相等/相差 1 ns、跨触点乱序、重复事件、未来事件阻塞后续事件、音频采样失败/映射变化、非法输入、队列满及 ReplayFull。
- 覆盖 pause/resume、seek 前后、后台/前台、Surface 重建和 EOF 余量边界。跨 epoch 事件不得误入新播放区间；UP/CANCEL 被拒绝时必须评估是否残留 active pointer，具体恢复策略在 S4 定稿。
- 设备缺失时真机验证 BLOCKED；模拟器只作补充。120 Hz 仅在实际支持时执行，否则记录 UNSUPPORTED_DEVICE 和设备证据，不将请求值当实际刷新率。离线核心验证无网络依赖。

## 6. 验收标准

| 编号 | 对应需求 | 可验证的通过条件 |
|---|---|---|
| PROD-INPUT-RELIABILITY-AC-001 | REQ-001 | 四种 phase 均有可关联轨迹；接受/拒绝/清空/pending 可对账；输入队列满、诊断满和导出失败分别可见；不完整轨迹不能报告完整复现 |
| PROD-INPUT-RELIABILITY-AC-002 | REQ-002 | 对本轮实际捕获的 LateInput 给出分类计数；至少一个确认缺陷有完整源轨迹、最小复现、对照和因果链；每项假设记录支持/反证/未解决，不将历史17/3次归因于未采集的事实。若本轮不能捕获对应真人原因，LI-001保持未完成 |
| PROD-INPUT-RELIABILITY-AC-003 | REQ-003 | 按 phase 对比原始、接受、判定和 pointer 生命周期；用具体事件号说明判定数量/顺序/误差、分数、active pointer 的变化或无变化；无法确定的影响明确保留，核心正确性问题升级 P0 |
| PROD-INPUT-RELIABILITY-AC-004 | REQ-004 | S4 冻结的输入策略及所有边界均有预期夹具；确认缺陷在旧行为复现，在修复后达到经审查的预期；残余拒绝逐类说明，不能仅以数量下降判定通过；无未处理 P0 |
| PROD-INPUT-RELIABILITY-AC-005 | REQ-005 | 完整原始轨迹按原调度重复100次，接受/拒绝决策与完整结果一致；旧Replay及修复后接受Replay在Headless重复100次和30/60/90/120 Hz回放调度下完整结果一致，并与设备记录一致；真实到达调度变体按S4支持范围判定，不把Replay的无分发延迟当成实时保证 |
| PROD-INPUT-RELIABILITY-AC-006 | REQ-005 | 真机完成单指、双指、密集MOVE、释放/CANCEL、暂停恢复、seek、后台/Surface、EOF及实际支持刷新率回归；真人和自动输入分开记录；既有核心/架构/实时路径/性能回归通过，证据关联真实版本与哈希；设备必要项不可由模拟器替代 |

## 7. 可追溯与证据

LI 编号继续由 [TASKS](../../../TASKS.md) 定义，不作为重复的 Legacy requirement ID。任务和实现位置见 [任务计划](tasks.md)，AC 到用例见 [测试计划](test-plan.md)。根因说明见 [调查报告](root-cause.md)，该文件当前只有事实来源、假设和待填证据；实际运行结果只写入 [validation](validation.md)。

正式证据将存于 `evidence/PROD-INPUT-RELIABILITY/<run-id>/`：保留原始轨迹、派生夹具、统计、设备结果、Headless结果、Replay、构建日志及 manifest。manifest 记录规格/设计/实现/验证器版本和产物哈希，使用稳定 Git 引用；当前不创建空 manifest 或虚构运行号。

## 8. 审查与未决项

本次完成文档层面的范围与追溯梳理，未执行自动检查。规格仍为 Draft，审批 Pending；调查方案具备规划基础，但以下事项使行为修复尚未达到实现门禁：

| 未决项 | 影响 | 关闭阶段 |
|---|---|---|
| 采集容量、内存预算、跨线程关联方案、离线工具接口及严格schema | 完整采集和可执行工具契约 | S1 |
| 真实 LateInput 根因及事件类型/判定影响 | 修复选择，是否升级P0 | S3 |
| 同批及跨帧排序范围、同时间稳定次序、允许延迟范围、提交水位和额外输入延迟上限 | 实时输入可接受范围及兼容性 | S4 |
| 迟到UP/CANCEL、重复DOWN、未来事件、EOF采样失败的处理方式 | pointer生命周期和有限结束行为 | S4 |
| 是否维持40 ms、是否需要版本语义变化 | 旧契约与新策略的差异 | S4 |

不得把这些未决项交给实现者自行选择后再补写已批准规格。

## 9. 变更记录

2026-09-20：建立 v0.1.1 文档草案；仅文档授权，代码和测试尚未开始，未产生验收结果。
