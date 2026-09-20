# HRK v0.1.0 软件验收记录

本轮交付范围：软件实现、主机自动测试、Harmony 原生/ArkTS 构建与未签名 HAP。按用户确认，真实设备验收保留待办；**不把本记录视为完整 Release Gate PASS**。

## 软件结果

|检查|结果|说明|
|---|---|---|
|MSVC 19.44 / Windows x64 Debug|PASS|完整主机构建、10/10 CTest 分组|
|MinGW GCC 14.2 / Windows x64 Release|PASS|独立编译器构建、10/10 CTest 分组|
|Harmony arm64 原生交叉编译|PASS|生成 `libhrk_entry.so`，链接真实 SDK 的 OHAudio/图形/NAPI 库|
|ArkTS + Native HAP|PASS|DevEco SDK 6.1.1 / API 24，完整 Hvigor 构建|
|HAP 内容|PASS|包含 native so、ArkTS bytecode、固定谱面、音频和 mixed Replay|
|架构边界|PASS|Kernel/Game/Platform source 扫描与 CMake 依赖图检查|
|100 次确定性重放|PASS|比较完整 JudgmentResult 序列和 ScoreResult|
|30/60/90/120/144 Hz、不规则帧、无渲染|PASS|同一 mixed Replay 均为 P/P/M/P、300 分|
|长帧输入|PASS|3 秒后一次消费整段已入队输入，与标准 Replay 一致，无帧龄丢弃|
|迟到输入修正|PASS|先产生超时 MISS 后到达合法原始 timestamp，重建为正确 PERFECT|
|Replay 磁盘 save/load|PASS|主机 CLI 保存后与输入 Replay 逐字节一致|
|100 次 Session 重建与生命周期压力|PASS|重复构建/重放、start/stop、pause/resume、seek|
|1000 个对象 AutoPlay|PASS|完整得到 1000 PERFECT|
|GitHub Actions|已配置，未远端执行|Windows/Linux 主机测试与 Linux ASan/UBSan job|
|真机 / 模拟器|NOT_RUN|本机 HDC 无连接设备；由用户确认本轮保留待办|

“10/10”是 CTest 分组数量，并不表示原需求所有设备用例都已执行。重复循环内的断言数量也不等价于独立测试用例数量。

## 自动化覆盖与需求对应

|CTest 分组/工具|覆盖内容|对应需求用例|
|---|---|---|
|hrk_base_tests|纳秒转换、唯一 ID、FIFO、溢出与并发队列|TC-BASE-001..003，TC-IN-006..007|
|hrk_time_tests|开始、暂停、恢复、seek、单调顺序、时间映射|TC-TIME-001..008|
|hrk_timeline_tests|单 BPM、变 BPM、双向转换、非法输入|TC-TL-001..005|
|hrk_input_tests|phase、原 pointer id、多指、CANCEL、1000 事件 FIFO|TC-IN-001..007|
|hrk_audio_tests|真实 WAV 解码、EOF 补零、PCM seek、FakeAudio 暂停恢复、资源失败|TC-AUDIO-001..006，TC-RES-002|
|hrk_replay_tests|头字段、二进制往返、版本/hash/rule 校验、截断/尾部/非法坐标拒绝|TC-REP-001..005|
|hrk_runtime_tests|音频驱动、原始输入时间、状态机、overflow 处理、生命周期压力、初始化失败|TC-AUDIO-007，TC-GAME-001..004，TC-STAB-001..003，TC-ERR-002..003|
|hrk_reference_game_tests|谱面校验、±50 ms 边界、位置、一次判定、固定 Miss、Score/reset|TC-CHART-001..004，TC-JUDGE-001..009，TC-SCORE-001..003，TC-ERR-001|
|hrk_determinism_tests|固定预期、100x、多帧率、无 GPU/音频设备、AutoPlay、seek、重建、长帧与迟到输入|TC-DET-001..005，TC-AUTO-001，TC-HEAD-001..003，TC-MEM-001，TC-PERF-003|
|hrk_architecture_tests|源码/API 边界、第三方头不泄漏、CMake 禁止依赖|TC-ARCH-001..005，TC-REN-002|
|verify_replay.py|读取完整 JSON 预期，比较所有判定字段/顺序和分数，实际保存 Replay 字节比对|TC-REP-003；支持后续 TC-E2E-005 的设备结果比对|

资源加载通过固定文件参与所有集成测试，RuntimeChart 公共接口仅返回 const 数据；NullRender 的 Sprite 提交由 runtime 分组检查。内存重建压力已运行，但**未宣称完成真机内存增长观察或本机 ASan 检查**。

## 证据与复现

```powershell
python tools/validate.py --config Debug --deveco "D:/ophm/DevEco Studio"
```

机器报告与日志位于 `out/validation/`：

- `report.json`：测试状态、源码/依赖 SHA-256、HAP SHA-256；`device=NOT_RUN`，`releaseReady=false`。
- `ctest.xml` / `ctest.log`：CTest 结果。
- `golden_replay.log`：固定预期及真实 Replay 文件往返比对。
- `harmony_hap.log`：Native/ArkTS/HAP 构建日志。

`docs/evidence/software-validation.json` 是本次交付的报告快照；后续改动必须重新运行验证，不得直接沿用旧报告。`--release-gate` 明确返回非零，避免把软件结果误当成完整设备 Release Gate。

所有未通过设备执行的 P0 与操作步骤见 [DEVICE_ACCEPTANCE.md](DEVICE_ACCEPTANCE.md)。本轮软件结果可以交付，正式 v0.1.0 发布仍需补齐这些证据。
