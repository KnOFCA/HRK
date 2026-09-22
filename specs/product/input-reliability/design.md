# HRK v0.1.1 输入可靠性设计

依据 [spec](spec.md)，系统架构见 [DESIGN](../../../DESIGN.md)。本文件不定义字段、阈值或门禁。具体修复决策等待 spec §3 的 S3/S4；没有已批准修复 ADR。

## 1. 模块与实现位置（计划）

| 模块 | 对应需求 | 计划改动位置与职责 |
|---|---|---|
| Harmony入口 | REQ-001/002 | `src/platform/harmony/backend.cpp`：关联回调批次、原始触点及接收时间，观察平台队列入口/失败 |
| 应用组合 | REQ-001/005 | `src/platform/harmony/entry.cpp`、`src/app/composition.cpp`：传递关联信息，控制采集和离线导出，关联生命周期 |
| 时钟映射 | REQ-001/002 | `src/kernel/time/time.h`、`time.cpp`：观察实际使用的采样、epoch、映射结果；不以接收时间替代事件时间 |
| 输入与Session | REQ-001/003/004 | `src/kernel/input/*`、`src/kernel/gameplay/session.*`：观察消费/暂存/拒绝/水位和pointer状态；修复位置由根因决定 |
| 诊断 | REQ-001 | `src/kernel/diagnostics/`：拟新增独立固定容量记录组件；不把Harmony类型泄漏进Kernel |
| Headless | REQ-002/005 | `src/platform/headless/*`、`tests/hrk_tests.cpp`：拟增加原始轨迹注入与时钟/调度控制；保持现有Replay运行路径 |
| 离线分析 | REQ-002/003/005 | `tools/`、`tests/data/`：拟新增解析、最小复现及差异报告能力；文件见 §6，CLI 见 spec §4.8 |

## 2. 数据流

平台回调 → 带关联号的原始事件 → 平台队列 → Session队列 → 实际时钟映射 → 输入提交/拒绝 → 判定与pointer状态。

旁路采集使用 spec §4 的关联关系，将各阶段记录写入预分配缓冲。platform 存储使用原子 fetch_add 预留独立槽，写完载荷后 release 发布 ready；每次回调先增加在途计数，再复查采集入口是否关闭，离开时递减。关闭入口后只在控制路径等待在途计数归零；新进入者复查关闭状态后退出，不访问冻结载荷。计数器使用饱和或溢出检查，不能回绕。导出 acquire 读取已提交槽。session 存储由现有 control 所有权域串行写入，不添加热路径阻塞锁。并发平台回调的完整性限制按 spec §4.6 执行，不假设输入 SPSC 队列支持多写者。

TraceContext 与事件使用同一队列槽，替代独立 sidecar FIFO（后者在 overflow/clear 时容易错位）。保留无诊断调用重载；仅内部 envelope 增加关联。队列 clear 必须观察实际被移除的事件号，pending 单独记录终态；已有 accepted 的事件不再次清空计数。原 clear 的线程边界和可见性必须保留，不能为了采集把并发新入队事件一起吞掉。已分配的采集状态在后端生命周期内持有，冻结及 reset 由控制路径确认写者退出，不在 callback 析构内存。

Headless 注入器按 spec §4.7 的 action 和实际 poll 关联恢复 Session 可见顺序，在每次公共音频查询时提供记录值。跨线程文件合并序号不是调度号；无法恢复队列可见性的轨迹拒绝完整复现。先重现旧行为再做单变量对照。只将派生对照输入排序，绝不覆盖原始记录；版本差异由报告标明。

## 3. 根因调查与候选修复

| 候选机制 | 调查方式 | 可能方案与限制 |
|---|---|---|
| 批内触点顺序不等于事件时间顺序 | 对照batch、pointIndex、映射时间和watermark | 对已知批次稳定排序；不能声称解决尚未送达的跨帧输入 |
| 分发延迟超出提交余量 | 比较receive/raw时间差、消费延迟和拒绝水位 | 有界重排/提交窗口；需权衡反馈延迟，先定义支持范围 |
| 单个接受事件推进全局水位 | 注入先到较新事件、后到较旧事件 | 分离可提交水位与已见时间；不能把已产生判定偷偷回滚 |
| 映射采样更新导致事件落到水位之前 | 固定原始事件，对照不同真实采样序列 | 明确采样有效性/映射策略；不凭空平滑掉证据 |
| 重复事件或生命周期清理 | 以事件号和epoch分析状态变化 | 按获批策略处理；相同时间/位置不自动等于重复 |

以上均为候选，具体根因状态见 [root-cause](root-cause.md)。不预先决定扩大余量、删除LateInput检查、改用handler时间或无限回滚。若新方案需要公共接口调整，在S4补充兼容设计及决策记录。

## 4. 失败处理与实时性

按 spec §5 处理缓冲满、输入丢失、采样失败、缺记录和导出失败。诊断失败只影响证据完整性，不更改游戏路径。离线拒绝畸形文件并指出记录号；不能通过插值修复后称作原始证据。

调试缓冲分配在load/控制路径，callback与update只追加有界记录。排序、JSON序列化、哈希和文件写入离开热路径。采集开关两种状态分别纳入性能计划；未测量前不声明开销可忽略。

## 5. 兼容性与剩余设计

旧Replay序列化、ReferenceGame规则、平台依赖边界保持现状。新诊断格式独立演进；原始轨迹复现与接受Replay重放分开实现和命名。

S1 已定稿存储预算、时间查询注入接口及导出快照同步，依据见 spec §4.4～4.8。S4 仍需定稿输入提交策略、pointer 恢复、EOF 以及版本行为。TASK-002 已实现采集并验证主机热路径分配；设备性能及真机行为尚未验证。

## 6. S1 实现分解与审查结论（2026-09-21）

TASK-002/003 条目已实现；实际验证分别见 validation：

| 任务 | 文件 | 实现职责 |
|---|---|---|
| TASK-002 | `src/kernel/diagnostics/input_trace.h/.cpp` | POD 记录、固定存储、饱和计数、冻结和离线序列化；预算由 static_assert 与分配计数验证 |
| TASK-002 | `src/kernel/input/input.h/.cpp`、`src/kernel/gameplay/session.h/.cpp` | envelope/pending 关联、真实分支/清理终态、pointer 快照；旧无诊断接口保留 |
| TASK-002 | `src/kernel/time/time.h/.cpp` | 观察生效 anchor/sample，保留 pending 的原映射关联 |
| TASK-002 | `src/platform/harmony/backend.h/.cpp`、`entry.cpp`、`src/app/composition.h/.cpp` | 回调批次、poll、顶层 action、既有控制清空、公共时钟查询观测；将采集控制与导出接到控制路径 |
| TASK-003 | `src/platform/headless/recorded_audio.h/.cpp`、`tools/input-trace/main.cpp` | RecordedAudioBackend、严格解析、调度 runner、差异和依赖闭包最小化；CLI 以 spec §4.8 为准 |
| TASK-002/003 | `tests/input_trace_tests.cpp`、`tests/data/input-trace/`、`CMakeLists.txt` | 独立测试目标及夹具，采用 C++17 有界 JSON 解析；现有 ReferenceGame 解析器为窄语法，不能直接充当轨迹解析器，不新增第三方运行时依赖 |

审查依据为现有 `backend.cpp`、`entry.cpp`、`session.cpp`、`time.cpp`、`headless.h` 及平台接口：

- frame 在 control 域内先 poll 后 update，touched 不持该锁；采集方案因而不依赖二者天然串行。
- OHAudio.position() 内部自行查询设备时间，Session.update() 又单独查询 clockSample；必须观察两种公共返回值，避免重现时漏掉时钟推进。
- start/resume/seek 建立 anchor；pending 缓存映射后的 InputEvent，因此再次消费不可重新映射。
- command 即使操作失败也会 input.clear，后台同时清 Session 和平台队列；只按成功 operation 推断清空会漏账，必须观察实际清理位置。
- ReplayFull 在当前事件尚未 feed 时触发 pause，EOF 可以留下尚未消费的输入；前者记录拒绝及实际清空，后者保留 pending，不发明恢复策略。

审查结论：调查方案可按现有模块边界实现，S1 无剩余契约阻塞；容量、实际并发和性能是否满足需由 TASK-002/003 及后续设备测试证明。此结论不是行为修复评审或运行验收。

## 7. TASK-002 实现与复核（2026-09-21）

- `InputTrace` 分开持有 platform/session 槽，平台回调整体持 Writer lease；冻结关闭入口后等待已有写者发布。导出按来源合并句柄；独立分类计数与保留记录逐项对账，缺记录、关联、边界或并发生产者均影响 complete。
- `TraceContext` 随内部 envelope 进入原容量队列；`FixedQueue::clearObserved` 固定一次 write 快照，不以循环 pop 吞掉新到事件。pending 保留首次 consume、mapped 和 sample 句柄；生命周期清理只对尚未终结事件结账。
- `TraceAudioBackend` 在 Session 的音频接口外观察实际 position/clockSample 返回，anchor 在 Session 的实际 mapper 调用旁记录。无需修改 ClockMapper/SongClock 算法或接口；不新增音频查询，未改变水位、窗口、Replay 或 pointer 规则。
- `Composition` 持有采集状态，Native 控制入口提供采集启动、冻结和路径导出。调用方在 load 前提供真实 provenance；余量来自当前音频后端。记录未知刷新率为 null。导出先写同目录临时工作目录，再以不覆盖的 hard link 发布最终文件，失败保留快照供重试。
- 复核发现原 enum 未覆盖既有 measure/120hz/未知命令清理，先在 spec requirements_version=3 补全观测类型；此项没有增加产品行为。空 poll 不产生转交 action。文件解析/原调度 runner 未在本任务实现。
- 测试实现为 `tests/input_trace_tests.cpp` 和独立 JSONL 导出检查 `tests/check-input-trace.mjs`；实际结果及预算见 validation。使用说明见 [采集验证入口](../../../tests/INPUT_TRACE.md)。

## 8. TASK-003 实现与复核（2026-09-21）

- 主机目标 `hrk_trace` 独立于 Harmony 和旧 Replay 路径。`json.h` 为有界 UTF-8/JSON 解析器，保留整数原文、拒绝重复键和非法 Unicode；`trace.h` 核对 schema、阶段/时钟/控制引用、epoch、批内顺序、终态与摘要。SHA-256 在离线工具内实现，不新增第三方运行时依赖。
- `RecordedAudioBackend` 按 action 顺序消费公共音频查询，查询类型、数量不匹配即失败。时长由哈希一致的 WAV 解码取得，余量来自 header。失败控制结果仅提供给音频后端；输入接受/拒绝仍由原 Session 实际计算。
- runner 从 receive/enqueue/poll/clear、epoch 和队列容量建立可见性约束，保留 FIFO、批内顺序与原始时间。按约束尽晚放入平台队列，未能满足约束则报告 TraceScheduleAmbiguous；该等价可见性调度不代表跨线程物理发生时间。平台输入队列不能成功入队的事件不会进入 Session。
- 每个动作执行后核对新采集的阶段、水位、映射、epoch、控制结果和 pointer 快照。重新执行的观测用 host/consume 墙钟不参与行为等价比较，原始事件/接收时间、anchor、position/clockSample 和 action 顺序保留。输出包含实际终态顺序、完整 Judgment/Timing Error/score、控制/update 边界和结束 pending。
- 最小化循环尝试删除单个事件或完整 action；每次重建序号引用、实际重放并核对同一目标拒绝，再验证生成轨迹可重放。达到单项删减固定点后保存源 runId、保留事件号、动作映射和删减步骤；不宣称全局最小。派生夹具标记 synthetic，不改写源文件。
- TEST-003 使用五组真实执行的合成对照，分别覆盖批内逆序、跨帧迟到、重复、映射变化和未来输入阻塞；它们证明工具和当前机制，不能关闭需要真人轨迹的 AC-002/LI-001。
