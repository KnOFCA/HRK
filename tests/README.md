# 产品测试

`hrk_tests.cpp` 使用固定夹具，逐条输出 TC 或回归用例编号与 PASS / FAIL，失败返回非零退出码。没有使用 assert 宏，因此 Release 构建仍执行检查。

CMake/CTest 同时运行产品测试和 `tools/check-architecture.mjs`。输入队列测试包括 100000 次双线程传输；确定性测试比较完整 JudgmentResult 序列。Windows 内存测试记录进程 PrivateUsage；CI sanitizer 检查为独立补充，不能将尚未运行的 CI 当作当前证据。

`data/` 保存 Chart、Replay 和 PCM16 WAV 固定输入；`expected/mixed_result.json` 保存固定端到端结果。
设备测试步骤见 [DEVICE](DEVICE.md)，预期唯一来源见 [验收契约](../specs/product/rhythm-kernel/acceptance.md)。
