# Harmony Rhythm Kernel（HRK）

C++17 音游内核原型：同一 ReferenceGame 通过接口组合 Headless 和 Harmony Backend。支持音频时钟、带时间戳的输入、BPM 时间轴、PERFECT/MISS、Play / Replay / AutoPlay 和跨帧率确定性重放。

v0.1.0 已完成实现与本轮验收（Accepted，未对外发布）：主机 92 项自动化测试通过，125 个原验收用例中 124 PASS、1 个条件性 120 Hz 用例记为 UNSUPPORTED_DEVICE。已完成 DevEco 模拟器验收及最终安装包的非人工真机补测，真机 60 秒基线最低 55 FPS；证据及限制见 [验证记录](VALIDATION.md)。

## 构建与测试

```powershell
cmake -S . -B build/host -G "Visual Studio 17 2022" -A x64
cmake --build build/host --config Debug
ctest --test-dir build/host -C Debug --output-on-failure
build/host/Debug/hrk_headless.exe tests/data/charts/simple_sequence.json tests/data/audio/short_test.wav tests/data/replay/mixed.replay
```

Linux 可用 `cmake -S . -B build/host -G Ninja`，随后 build / ctest；CI 额外启用 AddressSanitizer / UBSan。

```powershell
python tools/prepare-harmony.py
python tools/harmony/harmony_build.py --project src/app/harmony --deveco "<DevEco Studio 安装目录>" --no-repair-links
```

在 DevEco 打开 `src/app/harmony`，为应用配置开发签名后安装。设备相关签名仅保存在被忽略的本地 build-profile.json5；可共享配置在 build-profile.template.json5。不要提交证书、私钥或口令。

应用操作：选择 Play / Replay / AutoPlay 加载资源，点击“开始”。Replay 默认加载固定 mixed 夹具；保存过本次输入后则回放内存中的记录。“保存 Replay”同时保存 session.replay 和 session-result.json 到应用 filesDir，供 Headless 对比。

## 阅读入口

下一轮 [v0.1.1 输入可靠性文档](specs/product/input-reliability/spec.md) 已完成 TASK-001～003：调查契约、采集、原始调度重放与最小化工具；AC-001 和工具主机验证通过。真机根因调查及修复验收尚待后续任务。

| 内容 | 文档 |
|---|---|
| 开发协议 | [AGENTS](AGENTS.md)、[agent](agent.md)、[贡献指南](CONTRIBUTING.md) |
| 系统规格与架构 | [SPEC](SPEC.md)、[DESIGN](DESIGN.md) |
| 任务与验证 | [TASKS](TASKS.md)、[TEST_PLAN](TEST_PLAN.md)、[VALIDATION](VALIDATION.md) |
| v0.1.0 契约 | [功能规格](specs/product/rhythm-kernel/spec.md)、[原始 TC](specs/product/rhythm-kernel/acceptance.md) |
| 工具链与设备验证 | [TOOLCHAIN](docs/TOOLCHAIN.md)、[设备步骤](tests/DEVICE.md) |
| 历史基线 | [迁移记录](docs/migration/SDD_ARCHITECTURE_MIGRATION.md) |

治理检查：Git Bash 执行 `bash tools/sdd/check.sh`。产品测试结果不由治理检查替代。

本轮已按用户授权正式签收（保留已知问题）；Git 版本与后续优先整改见 [签收记录](docs/acceptance/v0.1.0.md)。
