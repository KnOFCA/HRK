---
id: PROD-INPUT-RELIABILITY
legacy_ids: []
title: HRK v0.1.1 输入可靠性与 LateInput 整改
domain: product
spec_status: Approved
approval: Approved
approver: 当前会话用户
approval_date: 2026-09-21
approval_evidence: 用户先后明确要求完成 v0.1.1 task001、task002、task003并提交已完成工作；授权调查工具实现与验证，不含 S4 行为修复批准
implementation_status: InProgress
validation_status: NotRun
release_status: NotAccepted
canonical_spec: specs/product/input-reliability/spec.md
requirements_version: 3
---

# HRK v0.1.1 输入可靠性规格

本文件是本功能需求、数据契约及验收条件的唯一事实来源。遵循 [系统规格](../../../SPEC.md) 和 [规格治理](../../README.md)。

## 1. 目标、范围与授权

完成 [LI-001～LI-004](../../../TASKS.md) 对应的原因定位、事件分类、判定影响评估、修复和真机/Headless 回归，使每个被拒绝事件能够被解释，并能从原始输入轨迹重现问题。

2026-09-20 用户仅授权编写配套文档，明确“不进行编码和测试工作”。2026-09-21 用户要求“完成`v0.1.1` task001”，授权完成 S1 调查契约、设计审查及相应文档验证；该授权作为 TASK-001 所要求的调查实现授权依据，仅覆盖本文件已定稿的诊断/注入契约，不批准尚未定义的行为修复。本次执行止于 TASK-001，不启动 TASK-002/003 的编码或设备采集。front matter 的 Reviewed/Approved 限于 S1；整个功能的实现、产品验证和签收仍未开始，S4 保持未通过。

历史事实：v0.1.0 真人两轮诊断记录 LateInput=17/3、overflow=0，已接受事件的 Replay 与 Headless 一致；被拒绝事件类型、根因及其判定影响仍未知。出处为 [签收记录](../../../docs/acceptance/v0.1.0.md) 与 [历史验证](../../../VALIDATION.md)，这些记录不算本迭代验证结果。

2026-09-21 用户进一步要求“完成`v0.1.1` task002”，授权按已定稿 REQ-001 实现全链路采集与 TEST-001/002 验证。上一段的停止边界为 TASK-001 历史执行范围；本次推进 TASK-002，不启动 TASK-003 原始调度工具或 S4 行为修复。

本迭代不包含校准 UI、音频偏移设置、Mania/Hold、新判定规则、3D 渲染、第三方游戏迁移或对外发布。保留 C++17、平台接口隔离和 ReferenceGame 规则。

2026-09-21 用户要求“完成`v0.1.1` task003并提交已完成的工作”，授权 §4.3/4.7/4.8 的原调度工具、解析验证、对照与最小复现及提交既有已完成工作；前述 TASK-002 停止边界为历史执行范围。TASK-004 真人调查和 S4 行为修复仍未开始。

## 2. 需求

| 需求编号 | 内容 | 优先级 | 验收 |
|---|---|---|---|
| PROD-INPUT-RELIABILITY-REQ-001 | 从平台接收至 Session 消费建立可关联的输入、时钟、控制操作与水位轨迹，提供按事件阶段和拒绝原因的统计；采集失败可见 | P1 | PROD-INPUT-RELIABILITY-AC-001 |
| PROD-INPUT-RELIABILITY-REQ-002 | 调查平台分发延迟、跨触点乱序、重复、映射变化和水位推进；用轨迹和对照实验说明已证实原因及未解释部分 | P1 | PROD-INPUT-RELIABILITY-AC-002 |
| PROD-INPUT-RELIABILITY-REQ-003 | 对照原始输入、接受输入、判定与触点生命周期，量化拒绝事件对判定、分数和状态的影响 | P1 | PROD-INPUT-RELIABILITY-AC-003 |
| PROD-INPUT-RELIABILITY-REQ-004 | 依据调查修复已证实缺陷，显式定义输入提交策略及异常行为；保留原始时间戳与既有判定窗口，不通过改为处理时间、扩大命中窗口或隐藏拒绝来通过验收 | P1；确认核心正确性缺陷后升 P0 | PROD-INPUT-RELIABILITY-AC-004 |
| PROD-INPUT-RELIABILITY-REQ-005 | 在 Headless、真机及既有 Replay 上验证确定性、生命周期和实时路径；归档可追溯证据 | P1 | PROD-INPUT-RELIABILITY-AC-005、PROD-INPUT-RELIABILITY-AC-006 |

## 3. 工作流与门禁

Workflow Step 是下表按依赖顺序执行的 S1～S6；Action Step 产生交付物，Validation Gate 检查通过条件，Recording Step 仅保存真实证据，不替代前置验收。

| 步骤 | 类型 | 产出与门禁 |
|---|---|---|
| S1 调查规格定稿 | 规格审查 | 明确采集容量、接口和回放调度格式，记录真实实现授权后才可编写诊断代码；2026-09-21 已通过，范围与授权见 §1/8 |
| S2 采集与复现 | Action Step | 实现采集/注入能力，保存完整运行与最小复现；区分真人、自动注入和合成夹具 |
| S3 根因与影响分析 | Action Step + Validation Gate | 按 AC-002/003 完成报告；未解释影响不得归为无害；核心正确性缺陷升 P0 |
| S4 修复规格与设计评审 | Validation Gate | 将 §8 未决策略补成唯一、可验证的契约，更新 requirements_version、设计和测试映射，记录真实授权；未通过不得实施行为修复 |
| S5 实现与回归 | Action Step + Validation Gate | 执行获批修复及 AC-004～006；已知 P0 或必要用例 FAIL/BLOCKED 均阻止验收 |
| S6 证据归档与签收 | Recording Step | 保存真实版本、运行和哈希；全部必要 AC PASS 后才可申请签收，不以报告写完替代验收 |

## 4. 输入与输出契约（S1 定稿）

### 4.1 输入及产物

输入是原始 Chart 字节、音频资源、原始平台事件、音频采样结果、Session 控制操作和 update 调度。Chart、音频、原始事件的既有合法范围引用 [v0.1.0 规格的实现前审查](../rhythm-kernel/spec.md)。

输出为独立诊断轨迹、统计摘要、最小复现、根因/影响报告、接受事件 Replay、完整判定结果与验证 manifest。诊断轨迹不混入游戏 Replay；Replay 只能证明接受事件的重放，不能替代原始轨迹。

轨迹采用离线导出的 UTF-8 JSON Lines，格式版本为 1。64 位整数使用十进制字符串，防止跨 JavaScript/Python 工具丢失纳秒精度。缺失或尚未产生的值写 null，不用 0 伪装采样成功。不同记录类型通过 kind 区分；字段语义见下表，严格形状、枚举和接口见 §4.4～4.8。

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
| countsByPhaseAndReason、captureDropped、pendingAtEnd、complete | 对象数组、无符号 64 位字符串、无符号 64 位字符串、布尔值；summary | 分类计数、诊断丢失量、未完成事件量及采集完整性 |

每个已捕获事件必须有唯一终态或显式 pending；总入口事件量与接受、拒绝、清空、pending 数量可对账。平台队列丢失、Session 队列丢失和诊断缓冲丢失分别报告，不合并为 LateInput。完整轨迹需覆盖关联 clock/control/update；缺任何必要关联或 captureDropped 非零时 complete=false，不可据此声称根因排他性或输入零丢失。

### 4.3 离线重现与差异输出

完整复现先按接收顺序、回调批次和 update 调度重建到达行为，并注入记录的时钟成功/失败结果；不得先排序原始轨迹从而消除待查问题。原始时间戳、采样值和 epoch 均保留。派生最小复现需记录来源事件号及删减步骤。

对照路径可使用按事件时间稳定排序的合成理想输入，但必须标注其假设及适用边界，不宣称等于设备上未发生的真实操作。输出包含拒绝事件号与原因、判定完整有序序列、Timing Error、分数，以及每个控制边界和运行结束时的 active pointers。报告给出首个差异位置；无法建立可靠预期时标记未解决。

无 input 的有效轨迹应能够重现纯超时，仍须具备 header、必要控制/时钟/update 和 summary；零字节文件无效。非法版本、缺必要记录、非法数值和损坏关联应拒绝“完整复现”，输出具体原因，不静默补齐。采集/导出失败不能改变游戏判定；专用诊断错误见 §4.8，不冒用既有 ReplayInvalid。

### 4.4 严格 JSONL 形状

以下约定与 §4.2 共同定义 schema，不另设第二份字段事实源。每行恰为一个对象，拒绝 BOM、空行、重复键、未知键、未知枚举、非有限数、截断行和额外记录；末行换行可有可无。所有列出的键必填，只有显式标注可空的值允许 null。不适用的键不得出现。字符串 UTF-8 编码；普通字符串上限 256 字节，路径上限 4096 字节。单行上限 1 MiB、文件上限 512 MiB，超限在分配大对象前拒绝。

`u64` 为 `0` 或无前导零的十进制正整数字符串，范围 0～18446744073709551615；`i64` 允许负号但不允许 -0，范围 -9223372036854775808～9223372036854775807。`u32` 为 JSON 整数 0～4294967295。所有序号从 1 开始，0 仅用于尚未建立的 epoch；时间均为 ns，host 使用设备 CLOCK_MONOTONIC 域，song 使用音频歌曲域。原始非法时间仍可记录为 i64，不能因游戏拒绝而丢弃证据。规范格式有效不代表事件满足游戏合法范围。

共同键为 `kind`、`recordSequence`；其余键按下表定义。header 第一行序号为 "1"，summary 最后一行，导出后 recordSequence 连续且唯一。运行记录另有 `source`（platform/session）、`sourceSequence`（u64，来源内递增）、`epoch`；关联使用明确的序号引用，不用行位置或时间排序猜测。导出时 platform 来源在前、session 来源在后，各自按 sourceSequence 排列；该物理文件顺序不代表运行调度。

| kind | 除共同键外的精确字段 |
|---|---|
| header | traceVersion=1、runId、provenance、captureConfig |
| input | 运行记录键；§4.2 中 eventSequence、batchSequence、pointIndex、pointerId、phase、position、rawHostTime、receiveHostTime、consumeHostTime、mappedSongTime、mappingSampleSequence、watermarkBefore、watermarkAfter、songNow、stage、disposition、error、reason；另有 rawPhase（u32）、actionSequence（u64 可空）、controlSequence（u64 可空）、updateSequence（u64 可空） |
| clock | 运行记录键；sampleHostTime、sampleSongTime、sampleSuccess；另有 query（position/clockSample/anchor）、caller（start/resume/seek/update/render/status/pause）、actionSequence（u64）、queryIndex（u32）、positionResult（i64 可空） |
| control | 运行记录键；operation、hostTime、targetSongTime、result；另有 actionSequence（u64）、boundary（begin/end）、activePointers（u32 数组，可空） |
| update | 运行记录键；updateSequence、hostTime、watermarkBefore、watermarkAfter、songNow；另有 actionSequence（u64）、boundary（begin/end）、result（既有错误名，可空）、activePointers（u32 数组，可空） |
| summary | countsByPhaseAndReason、captureDropped、pendingAtEnd、complete；另有 ingressCount、acceptedCount、rejectedCount、clearedCount（均 u64）、captureDroppedBySource（platform/session 两个 u64）、platformQueueDropped、sessionQueueDropped（u64）、pendingEventSequences（u64 数组）、incompleteReasons（§4.8 诊断错误名数组） |

`provenance` 精确键：sourceRevision、specRevision、policyVersion、buildMode（Debug/Release）、deviceModel、osVersion、actualRefreshHz（正有限数或 null）、chartSha256、audioSha256、origin（human/injected/synthetic）。版本为非空字符串；SHA-256 为 64 位小写十六进制。deviceModel/osVersion 在主机合成时填主机事实，不填虚构设备；刷新率未知须为 null，不能写请求值。

`captureConfig` 精确键：enabled（true）、platformCapacity、sessionCapacity、slotBytes、memoryBudgetBytes（u32）、hostClock（CLOCK_MONOTONIC）、timeUnit（ns）、inputDeliveryGrace（i64）、policy（v0.1.0-observe）。容量值来自 §4.6；余量记录被测后端实际返回值，不把 Harmony 参数强加给原 Headless。S4 新策略须修订此契约。

`position` 精确键 x/y，记录 RawInputEvent 的归一化 float 值，以可往返 float32 的 JSON 数值输出；NaN/正负无穷使用字符串 NaN/+Infinity/-Infinity，仅作为非法输入证据，解析后仍应触发原游戏检查。pointerId 为 u32；phase 为 DOWN/MOVE/UP/CANCEL，分别对应 rawPhase=0/1/2/3；非法 phase 原值保留在 rawPhase，phase=UNKNOWN，仅允许以 invalid_input 拒绝并单独计数，不混入四种合法阶段。batchSequence 使用 u64（修订 §4.2 的泛称），pointIndex 为 u32，两者同时非空或同时 null；Harmony DOWN/UP 的 pointIndex 为 0，MOVE/CANCEL 保留数组索引；无平台批次的合成 Session 输入才可为 null。

### 4.5 阶段、终态与关联

`stage` 枚举为 receive/platform_enqueue/platform_poll/session_enqueue/map/pending/terminal。`disposition` 枚举为 observed/queued/pending/accepted/rejected/cleared；前三项不是终态。receive 使用 observed，两个 enqueue 成功使用 queued，platform_poll/map 使用 observed，pending 使用 pending，terminal 只使用后三项。失败分支可直接产生 terminal。error 必须是既有 `errorName(Error)` 名称，成功/观察/清空为 Ok；reason 正常为 null，异常为下列唯一分支名。

| reason | 观测分支 |
|---|---|
| before_epoch / mapped_out_of_range | ClockMapper 的 epoch 检查失败 / 其余时间映射范围失败 |
| before_watermark | Session 映射时间小于当前水位 |
| platform_queue_full / session_queue_full | 对应队列 push 失败，分别累计 |
| invalid_input / invalid_state | 原输入检查 / 原状态检查拒绝 |
| replay_full | 接受 Replay 无剩余容量，当前事件以 rejected 结账；随后 pause 清空其余事件 |
| lifecycle_clear / inactive_drain | 控制操作实际清理队列或 pending / frame 在非 Playing 状态实际 poll 后丢弃 |

平台队列满的 error 映射为既有 QueueFull；其他拒绝保留原调用返回错误。所有阶段尚未产生的 consume/mapped/sample/watermark/songNow 值填 null；platform 阶段的 actionSequence/controlSequence/updateSequence 均为 null，Session 阶段 actionSequence 必填，控制清理关联 controlSequence，update 内处理关联 updateSequence，其他关联为空。导出可将内部稳定记录句柄解析为最终 recordSequence，必须保持同一目标，禁止以最近时间猜配。

终态仅记录第一次最终决策：accepted 在实际 feed 时，rejected 在实际拒绝分支，cleared 在真实清理/丢弃时。已接受事件后来 seek 或 stop 不重复结账。pending 每次暂存可记录，但 summary 只计算独立事件。平台入口先分配 run 内唯一 eventSequence；所有阶段保留完全相同的原始字段，消费前 consumeHostTime 为 null，首次 Session 消费时写入，pending 再处理沿用首次值；实际再次处理的时点由关联 update 确定。映射失败 mappedSongTime 为 null，映射成功后 pending 必须保留原值及原 mappingSampleSequence，不能暗中重新映射。

epoch 在成功 start/resume/seek 的 anchor 后递增，失败不递增，其他操作不递增；平台入口保留接收时观察到的 epoch，Session 阶段记录当时实际 epoch，不能据此替代原 hostTime 的 StaleInput 检查。mappingSampleSequence 引用实际生效的 clock/anchor 的 recordSequence；失败采样不更新关联。anchor 记录真实 host/song，sampleSuccess=true；clockSample=false 时两采样值和 positionResult 均为 null；position 查询仅 positionResult 非空且 sampleSuccess=true，表示函数返回可用数值，不意味着内部设备采样成功。clockSample=true 时仅采样对非空。不为记录增加任何额外音频查询。

control 的 operation 枚举为 load/start/pause/resume/seek/stop/background/foreground/surface_create/surface_resize/surface_destroy/render/status/measure/request_120hz/unknown_command；包含 render/status 是因为它们会读取 position()。measure、request_120hz 和 unknown_command 分别观察既有性能计数重置、120 Hz 请求和未识别命令，其原有成功/失败与随后平台队列清空均保留；不能伪装为 status 查询。每个顶层操作、每个 update 和每次成功平台 poll 转交分配一个连续 actionSequence，按 Session 所有者实际执行顺序递增；空 poll 不产生转交 action。内部 pause/stop 等嵌套副作用属于该 action，使用 controlSequence 关联具体 control begin 的 recordSequence。queryIndex 从 0 起按该 action 内公共音频调用顺序递增。updateSequence 按实际 update 调用递增。begin 的 result、activePointers 及未得出的水位/歌曲值允许 null；end 的 result、activePointers 必须非空，activePointers 升序且不重复。seek 以外 targetSongTime 必须为 null。begin/end 使用相同 actionSequence 和 operation/updateSequence。

countsByPhaseAndReason 为对象数组，每项精确键 phase、disposition、reason、count；只汇总终态，count 为 u64，不重复组合，省略零计数组合。ingressCount = acceptedCount + rejectedCount + clearedCount + pendingAtEnd；pendingAtEnd 等于 pendingEventSequences 的去重数量。四种合法 phase 及 UNKNOWN 均按此对账。overflow 属于 rejected，诊断丢失不属于游戏拒绝。summary 从保留轨迹核对实际独立计数器，不能丢记录后重新数剩余记录来假装总量完整。pending 存在可以 complete=true，只要列表及所有关联完整；复现必须保留该结束状态，不自动清空。

### 4.6 采集容量、所有权与导出

默认关闭诊断。在 load 前控制路径调用 beginCapture 开启一次运行；运行中不调整容量。platform 和 session 各 65536 个记录槽，每槽含对齐/提交标记的总大小不超过 256 bytes，记录载荷总上限 32 MiB；额外计数器、队列关联元数据、header/summary 工作区合计不超过 1 MiB，采集总预算 33 MiB（34603008 bytes）。原两个 2048 输入队列及既有 Replay 容量不变；添加的关联元数据计入上述额外预算。槽大小和预算在 TASK-002 用 sizeof/static_assert 及分配统计核验；这些是上限契约，当前无内存实测结果。

满时不覆盖、不扩容，后续记录分别累加 captureDroppedBySource，captureDropped 为两者之和；诊断失败不阻止游戏事件入队/消费。序号及计数器溢出时停止采集并标记 CaptureCounterOverflow，不回绕。预分配失败返回 CaptureAllocationFailed，保持诊断关闭和原游戏行为。header/summary 不占运行记录槽，满后仍可生成不完整摘要。不能保证任意长度真人轨迹完整；采集结束应先核对剩余容量及 dropped，需要更多证据时缩短单轮采集，不能静默提高预算。

平台回调可能与 frame/control 并发。采集存储支持有界无等待槽预留，使用原子索引和每槽 release 提交标记；不得把 SPSC 当多写者容器。Session 记录只由已有 control 所有权域内的执行者写入。诊断不得新增阻塞锁，也不得把 touched 改为获取 control 的阻塞锁。若平台回调存在并发生产者而现有输入队列不支持，记录 CaptureConcurrencyUnsupported 并令 complete=false，将其列入 S3 调查；不能在 S1 顺手改变队列线程契约。

beginCapture 只在无活动采集且未 Playing/Paused 时成功，否则 CaptureInvalidState。endCapture 只在非 Playing 状态冻结，且不代替游戏 pause/stop，不隐式清理 pending；在控制路径关闭记录入口、等待在途写者退出后取得不可变快照，等待最多 1000 ms，超时 CaptureBusy 并保留缓冲供重试。被排除在冻结边界之外的新输入不计入该 run。快照期间不释放在途写者可访问的内存；只有控制路径确认全部写者退出才能 reset/destroy。冻结不是销毁游戏 backend，输入仍按旧路径处理。

exportCapture 只接受冻结快照；排序合并、JSON 序列化、计算哈希及文件写入均在控制路径进行，不影响输入/音频/render 热路径。导出至临时文件，完整写入成功后生成最终文件；错误返回 CaptureExportFailed 并保留快照，可重试。不将半文件标为成功，不自动覆盖已有证据。导出 I/O 错误独立返回，不伪装为轨迹中的游戏错误。

### 4.7 注入接口与调度边界

诊断关联使用独立 TraceContext（run 内 eventSequence、batchSequence、pointIndex、receiveHostTime、接收 epoch），与 RawInputEvent 作为内部 envelope 同槽进出平台和 Session 队列；不靠时间戳/位置匹配，也不修改旧 Replay/RawInputEvent 的序列化。无采集调用保留原接口，诊断上下文可选；旧调用的行为不变。

Headless 专用 RecordedAudioBackend 实现现有 IAudioBackend；为每个 action 提供按 queryIndex 排列的 position() 和 clockSample() 返回值，以及控制操作的既有 Error 结果、duration 和 inputDeliveryGrace。load 的 Chart/音频必须与 header 哈希相同。不能用 FakeAudioBackend.advance() 推测录下的时钟，不能只重放 clockSample 而漏掉 position 返回值。嵌套 OHAudio position 内部查询可在平台边界观测，但核心重现注入其公共 position 结果；不再执行一次内部设备查询。

Runner 的逻辑接口为 validateTrace(trace)、runTrace(trace, chartBytes, audioBytes)、compareResults(expected, actual)。runTrace 按 actionSequence 调用原有控制、poll 转交与 update，按实际 platform_poll 记录恢复每个 frame 消费了哪些事件及其顺序；记录中 Session push 的返回结果只能作为比较预期，不能直接强制其接受/拒绝。platform 队列的成功入队/溢出和清空需分别核对原始入口及 poll/clear 关联。对回调与 poll 重叠的运行，只在证据能确定其队列可见性时宣称完整平台调度重现；无法确定时报告 TraceScheduleAmbiguous，保留 Session 消费顺序的局部复现，但 complete=false，不用人为串行顺序冒充真实发生顺序。入口未成功排入平台队列的事件不得直接注入 Session。

同一 action 的查询数量、类型、顺序必须与记录一致；缺查询、剩余查询、错类型均 TraceQueryMismatch，不外推、不沿用末值。同一原始事件各阶段的关联、唯一终态、批内顺序、begin/end、epoch 及 sample 引用必须通过预检；不接受断链或重复终态。原始 hostTime 不参与重排；同时间事件仍按记录的批次/poll/action 顺序处理。无 input 的纯超时轨迹走同一接口。

输出 result.json：固定键 complete（bool）、diagnosticError（§4.8 错误或 null）、decisions（eventSequence/disposition/reason/error 对象数组，实际终态顺序）、judgments（entity/type/targetTime/inputTime/error 对象数组，保持 emit 顺序；entity 为 u64、type 为 u32，时间为 i64）、score（value 为 u64，perfect/miss 为 u32）、boundaries（actionSequence/recordSequence/activePointers 对象数组，recordSequence 指向对应 control/update end）、pendingEventSequences（u64 数组）。compareResults 逐项比完整序列，输出相等或首个差异的 JSON 路径、expected/actual，不能仅比较哈希或总分。最小化工具保留源 runId、事件号、删除的 action/事件及依赖闭包说明，不修改原始文件。

### 4.8 CLI 与诊断错误

TASK-003 实现主机 CMake 目标 hrk_trace，源文件为 `tools/input-trace/main.cpp`；执行与验证说明见 [工具入口](../../../tests/INPUT_TRACE.md)。

```text
hrk_trace validate --trace <trace.jsonl> --out <validation.json>
hrk_trace replay --trace <trace.jsonl> --chart <chart> --audio <wav> --out <result.json>
hrk_trace compare --expected <result.json> --actual <result.json> --out <diff.json>
hrk_trace minimize --trace <trace.jsonl> --chart <chart> --audio <wav> --event <eventSequence> --out-dir <new-directory>
```

validate/replay 成功退出 0；compare 相同为 0、存在差异为 1；格式/关联/不完整/不支持的输入为 2；文件访问或工具资源失败为 3；minimize 仅在原轨迹完整且目标拒绝可复现时输出保留同一拒绝分支的依赖闭包夹具，否则为 2。不允许覆盖已有输出。失败 JSON 固定键 error、recordSequence（u64 可空）、field（字符串可空）、message（非空字符串）；退出码非 0 时不得留下貌似成功的 result。局部调查应使用独立报告，不输出 complete=true 的成功结果。

诊断错误枚举：TraceInvalidSchema、TraceUnsupportedVersion、TraceInvalidValue、TraceMissingReference、TraceIncomplete、TraceScheduleAmbiguous、TraceQueryMismatch、TraceAssetMismatch、TraceLimitExceeded、TraceTargetNotReproduced、CaptureInvalidState、CaptureAllocationFailed、CaptureFull、CaptureCounterOverflow、CaptureConcurrencyUnsupported、CaptureBusy、CaptureExportFailed、TraceIoError。按首个出错记录及字段定位；summary 的 incompleteReasons 去重记录所有已知采集/完整性问题。游戏 Error 与上述诊断错误属于独立类型。TraceIncomplete 包含缺 summary、未配对操作、缺阶段、总量不符等情况，不能因工具忽略字段而通过。

## 5. 兼容性、资源与异常

- 现阶段不修改 [v0.1.0 输入水位契约](../rhythm-kernel/spec.md)。本草案不是另一套已生效行为；S4 应明确列出被替换条款、适用版本和旧行为回归范围。
- 保留 ReferenceGame 命中窗口、计分、Chart 原始字节哈希与旧 Replay 格式；旧固定 Replay 的完整结果必须保持一致。若最终策略必须改变版本语义，应在 S4 明确 kernelVersion/兼容范围，不能静默改变。
- 保留原有输入队列容量和满时策略，除非 S4 有明确需求变更。采集缓冲独立预分配，容量及内存预算见 §4.6；热路径不新增堆分配、文件 IO、排序导出、阻塞锁或日志洪泛。导出放在暂停/停止后的控制路径，不引入第三方运行时依赖。
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

2026-09-21 执行 Agent 对 S1 进行规格与代码可行性审查：§4.4～4.8 已明确严格格式、完整性、预算、并发冻结及时钟注入；不改变现有游戏行为，调查契约 Reviewed，授权来源见 §1。审查检查表及治理检查证据见 [validation](validation.md)。S1 审查时未运行产品测试；TASK-002 采集与主机验证记录见 validation，以下 S3/S4 未决项仍使行为修复不具备实现门禁：

| 未决项 | 影响 | 关闭阶段 |
|---|---|---|
| 采集容量、内存预算、跨线程关联方案、离线工具接口及严格schema | 已关闭，契约见 §4.4～4.8；运行正确性由后续 TEST-001/002/003 验证 | S1 已通过 |
| 真实 LateInput 根因及事件类型/判定影响 | 修复选择，是否升级P0 | S3 |
| 同批及跨帧排序范围、同时间稳定次序、允许延迟范围、提交水位和额外输入延迟上限 | 实时输入可接受范围及兼容性 | S4 |
| 迟到UP/CANCEL、重复DOWN、未来事件、EOF采样失败的处理方式 | pointer生命周期和有限结束行为 | S4 |
| 是否维持40 ms、是否需要版本语义变化 | 旧契约与新策略的差异 | S4 |

不得把这些未决项交给实现者自行选择后再补写已批准规格。

## 9. 变更记录

2026-09-20：建立 v0.1.1 文档草案；仅文档授权，代码和测试尚未开始，未产生验收结果。

2026-09-21：requirements_version=2；完成 TASK-001/S1 调查契约与设计审查，记录用户授权。未批准 S4 修复策略，产品状态保持 NotStarted/NotRun/NotAccepted。

2026-09-21 TASK-002：用户授权采集实现与验证；requirements_version=3 补足既有 measure/120hz/未知命令的观测枚举，明确空 poll 不产生转交 action。此项为既有行为的诊断契约补全，经源码与导出测试审查；不改变输入策略或旧命令结果。AC-001 验证见 validation。
