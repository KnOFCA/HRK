# HRK v0.1.1 测试计划（未执行）

需求和通过条件仅由 [spec](spec.md) 定义，设计见 [design](design.md)，实际结果写入 [validation](validation.md)。本文件仅规划未来测试；用户明确要求本次不编码、不测试。

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

实施授权后依次执行：构建 → 架构/静态检查 → 单元/轨迹测试 → Replay回归 → 真机 → 逐AC验收 → 证据归档。以下仅为现有命令参考，本次未执行；新增轨迹工具命令须在S1定稿后补入，不虚构可运行CLI。

```powershell
cmake -S . -B build/host -G "Visual Studio 17 2022" -A x64
cmake --build build/host --config Debug
ctest --test-dir build/host -C Debug --output-on-failure
node tools/check-architecture.mjs
```

Harmony构建与设备步骤引用 [工具链](../../../docs/TOOLCHAIN.md) 和 [DEVICE](../../../tests/DEVICE.md)。Linux sanitizer依据现有CI流程执行并保存实际日志；不能从配置存在推定通过。

正式实现交付需执行 `bash tools/sdd/check.sh` 并记录结果；本次依用户“不进行编码和测试工作”的明确限制不运行，其中包含工具自测和模板演练。治理检查不替代产品验收。

## 5. 结果记录

执行后逐用例与AC记录PASS/FAIL/BLOCKED及证据。未开始保持NotRun。条件性设备不支持按spec §5记录；已知P0、必要设备项缺失及未解释影响不得计为整体验收通过。
