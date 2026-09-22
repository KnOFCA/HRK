# v0.1.1 采集入口与主机验证

契约见 [输入可靠性规格](../specs/product/input-reliability/spec.md) §4，结果见 [validation](../specs/product/input-reliability/validation.md)。此入口覆盖 TASK-002 采集与 TASK-003 原始调度 CLI。

主机执行：

```powershell
cmake --build build/host --config Debug
ctest --test-dir build/host -C Debug --output-on-failure
```

`hrk_input_trace_tests` 由 Node 测试入口启动，生成合成 Chart/WAV、真实主机与源码/规格摘要，再运行 C++ 测试和独立导出检查。每次轨迹保存在新的 `build/host/input-trace-tests/run-*` 目录；它们是单元测试轨迹，不是本轮真人根因证据。输出末尾给出实际目录。大容量溢出文件留在 build，不作为设备轨迹使用。

Harmony 调试调用顺序：

1. 在未 Playing/Paused 且 load 前准备 [InputCaptureMetadata](../src/app/harmony/entry/src/main/cpp/types/libhrk_entry/index.d.ts)。填入实际源码/规格版本、构建模式、设备/系统、Chart/WAV 的 SHA-256、来源类别及实测刷新率；未知刷新率填 null。原始素材必须与 initialize 使用的字节相同。
2. 调用 Native `beginCapture(metadata)`，确认返回 `Ok`，再使用原有 initialize/command 操作。采集默认关闭，不自动开始游戏。
3. 暂停或自然结束后调用 `endCapture()`。该调用不替代 pause/stop，也不隐式清空 pending；`CaptureBusy` 时保留状态并稍后重试。
4. 调用 `exportCapture(filesDir + '/新的运行名.jsonl')`。检查返回值；已有路径不覆盖，失败可用新的可写路径重试。不要在回调或帧处理路径导出。
5. 核对 summary 和 provenance。非完整采集不能用于排他性归因或完整复现；缩短采集时长后重采。接受事件 Replay 仍通过原 exportReplay 单独保存。

Native 接口已经编译验证；TASK-002 未运行本轮设备采集或性能测量。真实设备按 [DEVICE](DEVICE.md) 及功能 TEST-007/008 在后续任务验证。

## 离线 CLI

主机构建生成 `build/host/Debug/hrk_trace.exe`（其他生成器按实际输出目录）。从仓库根目录执行以下命令，将占位路径替换为同一次采集的原始素材：

```powershell
build/host/Debug/hrk_trace.exe validate --trace trace.jsonl --out validation.json
build/host/Debug/hrk_trace.exe replay --trace trace.jsonl --chart chart.json --audio audio.wav --out result.json
build/host/Debug/hrk_trace.exe compare --expected expected.json --actual result.json --out diff.json
build/host/Debug/hrk_trace.exe minimize --trace trace.jsonl --chart chart.json --audio audio.wav --event 2 --out-dir minimal
```

输出必须使用新路径。minimize 保存 trace.jsonl、result.json、minimization.json 及原始素材；可直接对派生 trace 执行 replay。它保留源事件号和目标拒绝，记录实际删减过程。工具不会排序原始事件或代替 Session 决定接受/拒绝。退出码、失败 JSON 和完整性要求以 spec §4.8 为准。

`hrk_trace_cli` CTest 项生成合成轨迹，执行严格解析、所有 CLI 路径、五组单变量对照、结果重放一致性及最小化再重放。日志给出新的 `build/host/trace-cli-tests/run-*` 与 `cli-*` 目录。对照中的稳定事件时间顺序只是假定理想到达的合成输入，不代表设备上真实发生的操作。
