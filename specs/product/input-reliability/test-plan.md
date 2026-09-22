# HRK v0.1.1 测试计划

需求和通过条件仅由 [spec](spec.md) 定义，设计见 [design](design.md)，实际结果写入 [validation](validation.md)。2026-09-21 TASK-001 执行文档审查；TASK-002 已执行 TEST-001 和 TEST-002 的采集范围、主机回归及 Native 编译。TASK-003 已执行外部解析、原始调度、五组合成对照及最小化测试；设备测试仍属后续任务。

## 1. 用例与验收映射

| 测试编号 | 验收 | 方法与场景 | 预期来源 |
|---|---|---|---|
| PROD-INPUT-RELIABILITY-TEST-001 | AC-001 | 为DOWN/MOVE/UP/CANCEL分别注入接受、暂存、拒绝和生命周期清空事件；核对入口号、时钟关联、水位和分类计数 | spec §4、AC-001 |
| PROD-INPUT-RELIABILITY-TEST-002 | AC-001 | 空轨迹、队列容量边界及超限、诊断满、文件写入失败、缺关联、非法版本/数值、未结束采集；检查游戏结果不受诊断失败影响 | spec §4/5、AC-001 |
| PROD-INPUT-RELIABILITY-TEST-003 | AC-002 | 原始轨迹按原调度注入，随后做单变量对照：批内触点时间倒序、跨帧迟到、重复、映射变化、未来事件阻塞；形成最小复现 | spec §4.3、AC-002 |
| PROD-INPUT-RELIABILITY-TEST-004 | AC-003 | 逐事件关联原始/接受输入；对比完整Judgment、Timing Error、score、active pointers；分别检查丢失DOWN和丢失UP/CANCEL，不能只测最终分数 | spec §4.3、AC-003 |
| PROD-INPUT-RELIABILITY-TEST-005 | AC-004 | 对S4定稿策略制作期望夹具：水位相等及±1 ns、同时间事件、重复DOWN、跨epoch、未来事件、采样失败、EOF余量边界；旧版本复现问题，修复版本验证预期 | spec §3 S4、AC-004；参数在S4冻结后补齐 |
| PROD-INPUT-RELIABILITY-TEST-006 | AC-005 | 分别执行原始轨迹固定调度重复、旧/新接受Replay重复及不同帧率重放；对比完整序列与分数，而非仅哈希或总分 | spec AC-005 |
| PROD-INPUT-RELIABILITY-TEST-007 | AC-002/003/006 | 真机真人单指命中、双指交错、密集MOVE后分别释放；另行自动注入与生命周期测试；每轮保存轨迹、Replay和结果并与Headless对照 | spec AC-002/003/006 |
| PROD-INPUT-RELIABILITY-TEST-008 | AC-006 | 既有主机核心/架构/Replay回归、Linux sanitizer、真机采集开关两种性能基线、热路径分配检查；设备支持时检查高刷新率 | spec §5、AC-006；既有指标引用v0.1.0规格 |
| PROD-INPUT-RELIABILITY-TEST-009 | §3 S1（文档门禁，非产品 AC） | 对照现有队列/Session/时钟代码审查 schema、空值、关联、容量预算、并发冻结、注入调度和错误接口；确认用户授权范围；执行完整 SDD 检查 | spec §1、§3 S1、§4.4～4.8 |

TASK-002/003 实现后对 TEST-001/002/003 增补以下执行覆盖：

- 分别达到 platform/session 槽容量边界及多一条；保留最早记录、累计各来源 dropped、摘要仍可导出；用 sizeof 和分配统计核对预算；关闭采集与采集溢出两种路径对比游戏完整结果。
- 覆盖 receive → enqueue → poll → map → pending → terminal，以及 enqueue 失败、非 Playing drain、失败 command 后 clear、ReplayFull 内部 pause、EOF 保留 pending；校验每事件唯一终态和总量对账。
- 合法无 input 轨迹、最大/最小 i64/u64、超过范围、重复键、未知键、错 null、重复终态、缺 begin/end/sample、缺 summary、过长行、错误哈希及无效版本；拒绝原因和定位需匹配 spec §4.8。
- 写者预留槽后暂停，控制路径开始冻结，随后写者提交；同时验证冻结超时、重试、导出失败重试及安全销毁。并发平台生产者不符合输入队列契约时不能宣称 complete。
- 原始时间逆序、相同时间、跨 batch/poll、pending 跨 update 不重新映射；在同一 action 分别给 position 与 clockSample 不同值及失败，缺少/多余/错类型查询须明确失败。
- CLI 成功、差异、非法/不完整输入和 I/O 失败的退出码；不得覆盖现有输出；最小夹具保留目标拒绝、源事件号及依赖，完整结果差异报告包含首个不同字段。

## 2. 调查用合成场景

以下是待实现的测试设计，不是已采集输入或已运行结果。

- 在同一epoch中，水位先低于1000 ms，同批两个pointer事件的映射时间依次为1020 ms、1010 ms；观察第一事件推进水位后第二事件是否拒绝。交换数组顺序作对照，固定其余条件。
- 固定原始时间及命中位置，只改变接收批次/update调度，跨越现有提交余量边界，区分分发延迟与玩家实际晚点。
- 固定原始事件与到达顺序，只改变注入的已记录音频采样；检查映射变化是否改变拒绝位置。
- 先DOWN再构造迟到UP/CANCEL，检查终态pointer；对照暂停/seek清理前后状态，不用ReferenceGame只关注DOWN的特性掩盖生命周期影响。

每个场景需在未来夹具中指定Chart原始字节、音频时间序列、控制操作、事件到达/消费顺序及期望值。修复后的预期由S4定稿，不在测试实现时自行选择。

## 3. 真机执行流程（未来）

1. 记录设备、系统、实际刷新率、源码/规格/策略版本、构建模式和安装包哈希，确认使用完整采集配置。
2. 按既有性能准备口径加载资源。真人单指、双指和密集MOVE分别运行；保留事件号与操作说明，不能以自动注入替代真人。
3. 分别执行暂停恢复、seek、后台/前台、Surface重建；构造跨epoch与EOF附近输入。自动触发CANCEL的用例标注来源；设备未产生CANCEL不能算该项已覆盖。
4. 停止或暂停后导出轨迹与结果；核对complete、captureDropped、pending和输入分类对账。采集不完整则补采，不用其作排他性归因。
5. 将同一轨迹交给原始调度重现路径，将接受Replay交给既有Headless路径，分别比较；回归结论与根因说明分开记录。
6. 对照采集开关运行性能基线；记录全部帧，不删除慢帧。高刷新率记录实测值；缺设备时必要真机项BLOCKED。

## 4. 执行层次和命令入口

产品实施时依次执行：构建 → 架构/静态检查 → 单元/轨迹测试 → Replay回归 → 真机 → 逐AC验收 → 证据归档。以下为现有产品命令参考，TASK-001 未执行；TASK-003 已实现轨迹工具，命令见 spec §4.8。

```powershell
cmake -S . -B build/host -G "Visual Studio 17 2022" -A x64
cmake --build build/host --config Debug
ctest --test-dir build/host -C Debug --output-on-failure
node tools/check-architecture.mjs
```

Harmony构建与设备步骤引用 [工具链](../../../docs/TOOLCHAIN.md) 和 [DEVICE](../../../tests/DEVICE.md)。Linux sanitizer依据现有CI流程执行并保存实际日志；不能从配置存在推定通过。

TASK-001 交付执行 `bash tools/sdd/check.sh` 并记录结果，其中包含工具自测和模板演练。Windows 使用 Git for Windows 自带 bash；治理检查不替代产品验收。后续实现交付仍须按贡献规范完成对应产品测试。

## 5. 结果记录

执行后逐用例与AC记录PASS/FAIL/BLOCKED及证据。未开始保持NotRun。条件性设备不支持按spec §5记录；已知P0、必要设备项缺失及未解释影响不得计为整体验收通过。

## 6. TASK-002 执行映射

`ctest --test-dir build/host -C Debug --output-on-failure` 包含 `hrk_input_trace_tests`：C++ 验证采集分支，Node 独立读取导出 JSONL 检查字段、空值、原始字段一致性、连续 action/query 序号、sample/control 关联和分类对账。每次输出到新的 build 子目录，避免覆盖既有证据。

- TEST-001：四 phase 的 accepted/rejected/cleared/pending；全链路、首次消费/原映射保留、epoch、pointer 边界、公共音频成功/失败查询及实际控制清理。
- TEST-002（采集范围）：默认关闭、非法状态/原始输入、空采集/纯超时、两个输入队列满、两个诊断缓冲满、预算/热路径分配、冻结提交/超时/销毁、并发生产者、counter overflow、缺关联/边界、导出失败/重试/不覆盖、EOF pending、ReplayFull 内部 pause；诊断关闭/开启/溢出逐项比较完整游戏结果。
- TEST-002 中外部文件的非法版本、重复键、超长输入、损坏数值等解析拒绝，按 design §6 分工留给 TASK-003；本任务不以导出器测试冒充尚未实现的解析工具验收。
- 既有 `hrk_tests`、架构与固定 Headless Replay 一并回归；Harmony Native 编译确认接入。TEST-007/008 中的设备性能和 Linux sanitizer 尚待后续回归，不作为本任务已执行项。

## 7. TASK-003 执行映射

`hrk_trace_cli` CTest 项调用 `tests/check-trace-cli.mjs`，生成独立合成素材/轨迹并运行真正的 C++ CLI；测试保留在新的 build 目录，不覆盖证据。

- TEST-002 外部解析范围：严格 UTF-8/JSON、重复键/未知字段/枚举/null、64 位上下界及越界、空/截断/超长行、缺 summary、阶段/批次/sample 断链、计数不符、资产哈希不符、公共查询错类型、无法放置的 epoch。失败检查退出码和定位错误；I/O、已有输出和非法结果文件独立覆盖。
- TEST-003：重放全部完整采集合成文件，包括两个队列满、时钟成功/失败、跨 epoch、pending 原映射、纯超时和 EOF pending。五组 original/control 仅分别改变批内时间顺序、分发帧、重复输入、采样映射或未来事件时间；对比完整结果并重复同轨迹检查确定性。
- 最小化：对批内逆序的拒绝 event 2 生成依赖闭包；验证确实删除无关 status action、保存源映射，派生 trace 再次重放仍为 before_watermark。无目标拒绝不生成成功目录。
- 本任务完成工具测试，不替代 TEST-004/007 的真人原因调查，也不提前宣称 AC-005 的 100 次/多帧率整体验收。
