# HRK v0.1.0 验证记录

2026-09-20 结果：**PASS**。实现 Complete，验收 Accepted，未对外发布。用户于 2026-09-19 授权完成迭代，2026-09-20 明确要求移除真机后使用 DevEco Studio 模拟器继续。

## 用户签收授权与交付执行

用户明确指示：“提交git并推送远程，然后签收本次迭代，把 LateInput 的原因、事件类型及判定影响列入后续优先整改项。”

签收范围为 v0.1.0 本轮实现与验收证据，保留已披露的 LateInput 和设备刷新率限制；不作零丢弃或无缺陷承诺。整改已登记在 [后续优先任务](TASKS.md)。实现基线 `8e45be80d4acfcb6454859bd2e6c21717d4f14ec` 已成功推送至用户指定远程 `origin/master`；现按该授权正式签收，状态 Accepted（保留已知问题）。签收范围、版本引用与整改责任见 [签收记录](docs/acceptance/v0.1.0.md)。

## 真人补测（同日最终）

真人输入、听感与旋转补测完成，13 个关联 TC **PASS**。实际操作由用户完成，未注入事件代替真人。

- 第一轮用户确认“保存成功、出现 PERFECT，声音/触摸无异常。”记录 1129 个事件，1 PERFECT / 3 MISS / 100 分；两个 pointer 均包含完整 DOWN/MOVE/UP，曾同时按下。最后一组手势跨过歌曲 EOF，先前两组双指手势已完整结束。
- 第二轮用户确认“暂停/恢复声音、旋转后的画面与触摸正常。”记录 710 个事件，两个 pointer 都在 EOF 前松开，末尾 active 集合为空。该轮用于暂停恢复和旋转，未要求再次命中音符。
- 两轮完整判定结果均与 Headless 重放逐项一致。
- 诊断仍记录 LateInput **17 / 3**，两轮 overflow=0。用户体验确认正常不等于零交付拒绝；此项作为既有输入水位契约下的限制保留，不声称输入无丢弃。
- 测试后已停止应用、移除真机 HDC TCP 连接，并再次确认 `list targets` 为 `[Empty]`。

证据：[真人 manifest](evidence/PROD-HRK/2026-09-20-human-final/manifest.json)、[逐项结果](evidence/PROD-HRK/2026-09-20-human-final/acceptance-results.json)。原真机自动补测及模拟器记录继续保留，以下“排除真人”仅描述较早轮次。

## 真机补测（同日后续）

按用户指令补完最终 HAP 的非人工真机测试，**PASS**；本轮 22 个关联 TC 中 21 PASS、1 UNSUPPORTED_DEVICE。真人触摸、听感及人工旋转复测按要求排除，自动注入只证明平台输入链路。

- 真机 MRO-W00 / OpenHarmony-6.1.1.120；安装包与上一轮归档 SHA-256 完全相同，产品源码未改动。
- 60 Hz：连续 60.016 s、3597 个帧间隔、p95=16.6707 ms、最低每秒 55 FPS，通过原性能门槛。
- 请求 120 Hz 后实际约 90 Hz：连续 60.0107 s、p95=11.1205 ms、最低每秒 84 FPS；条件性 120 Hz 用例仍为 UNSUPPORTED_DEVICE。两个刷新率下 124 个判定逐项相同，均与 Headless 一致。
- 自动 DOWN/UP/MOVE 和双指注入：178 个记录事件、pointer 0/1、2 PERFECT / 2 MISS；完整 Replay 等于 Headless。5 个超出交付水位的合成事件被拒绝，overflow=0，不将其描述为人工输入结果。
- 暂停 5.034 s，2 s 后不变；恢复后 5.745 s；seek 后 4.290 s。后台/前台和 Surface 重建均保持 4.634 s，创建/销毁/尺寸变化计数为 2/1/1。
- 固定 mixed Replay 为 3 PERFECT / 1 MISS / 300 分，与预期和 Headless 完整结果一致。
- 已停止测试应用，执行 HDC `tconn <真机端点> -remove`；随后 `list targets` 返回 `[Empty]`，确认真机调试连接已断开。

本轮 [真机 manifest](evidence/PROD-HRK/2026-09-20-physical-final/manifest.json)、[逐项结果](evidence/PROD-HRK/2026-09-20-physical-final/acceptance-results.json)、[断开连接证据](evidence/PROD-HRK/2026-09-20-physical-final/physical-final-disconnect.json)。脚本增加 `--prefix physical-final`，保留原模拟器证据；修正了证据路径拼接和无线命令延迟导致的测试时序问题，未放宽产品判定或性能门槛。

以下为较早完成的模拟器及主机基线，保留其原始范围和结果。

## 实际执行

| 项目 | 结果与证据 |
|---|---|
| 主机 CMake / MSVC Debug | 构建通过；C++ /W4 /WX |
| 主机自动化 | 92 PASS、0 FAIL，覆盖 81 个原 TC 及额外回归断言 |
| CTest | 3/3 PASS（主机、架构、CLI 完整结果） |
| 架构静态检查 | 6 个原 TC PASS；Game / Kernel / Platform 依赖隔离 |
| Harmony Hvigor | ARM64 + x86_64 签名 HAP 构建与模拟器安装 PASS |
| 原生输入 / Replay | 真机 56 个手指事件；模拟器 88 个平台注入事件，含 pointer 0/1 和 DOWN/MOVE/UP；各自完整结果等于 Headless |
| PERFECT / MISS | 模拟器 native touch 得到 1 PERFECT、3 MISS；mixed fixture 为 3 PERFECT、1 MISS、300 分 |
| 生命周期 | 5.158 s 暂停 2 s 不变；恢复至 6.331 s；seek 至 4.000 s；后台/前台和 Surface 重建保持 4.221 s |
| 最终性能基线 | 模拟器连续 60.0027 s，3590 帧间隔，p95=16.6744 ms，最低每秒 57 FPS；124 PERFECT / 0 MISS |
| SDD 治理 | 完整 check.sh PASS；107 个链接、22 个验证工具测试、43 个必需输入；FAIL=0 / BLOCKED=0 |
| 原验收矩阵 | 125 项：124 PASS、1 UNSUPPORTED_DEVICE（条件性 TC-PERF-002） |

命令：

```powershell
cmake --build build/host --config Debug -j 8
ctest --test-dir build/host -C Debug --output-on-failure
python tools/harmony/harmony_build.py --project src/app/harmony --deveco "D:/ophm/DevEco Studio" --no-repair-links --skip-install
python tools/test-device-input.py --hdc "D:/ophm/DevEco Studio/sdk/default/openharmony/toolchains/hdc.exe"
python tools/test-device-lifecycle.py --hdc "D:/ophm/DevEco Studio/sdk/default/openharmony/toolchains/hdc.exe"
python tools/archive-validation.py --device-results build/device/acceptance-results.json --run-id 2026-09-20-final
```

工具链：MSVC 19.44.35228、CMake 3.27.3、Harmony SDK Clang 15.0.4。真机 MRO-W00 / OpenHarmony-6.1.1.120；最终模拟器 MatePad Pro 12 / OpenHarmony-6.1.1.125 / x86_64，屏幕 2800×1840、Native Surface 2728×806。最终 HAP 为本地开发签名，仅适用获授权的测试环境。

## 证据与版本

完整 [manifest](evidence/PROD-HRK/2026-09-20-final/manifest.json)、[逐 TC 结果](evidence/PROD-HRK/2026-09-20-final/acceptance-results.json)、[主机结果](evidence/PROD-HRK/2026-09-20-final/host-results.json)。规格、设计、源码、验证器及 HAP / SO 的 SHA-256 已归档，未伪造 commit 或 tag。签名凭据不入库。

本地产物为 build/artifacts/hrk-v0.1.0-device-tested.hap，含 ARM64 和 x86_64。对应原生库也保存在 build/artifacts，产物本身按系统规格不入库。

## 已知限制与早期失败

- 真机用户确认声音正常、修复后触摸计数正常、旋转正常。最终性能验收在用户指定的模拟器完成；未声称最终包重新通过真机性能测试。
- 早期真机最低 17 FPS、模拟器最低 44/45 FPS 的探测未通过，原始结果保留。资源准备后才启动播放与测量；最终模拟器独立运行期间无本机搜索/构建，全部 60 秒纳入统计，未剔除慢帧。
- 120 Hz 请求成功但模拟器实际仍约 60 Hz，先前真机约 90 Hz；按条件用例记录 UNSUPPORTED_DEVICE，不推断面板硬件上限。
- 模拟器密集注入有 15 个超过 40 ms 交付水位的 LateInput，88 个事件被记录且重放精确一致。真实手指复测为零 LateInput。LateInput 是分发过迟，不是玩家晚点；超过命中窗口由 MISS 表示。
- Hvigor 全量 PreBuild 对合法版本 0.1.0 发出 SemVer 警告：本机插件正则要求主版本非零。保留实际版本，未修改工具或隐藏警告；C++ 编译无 warning，最终增量构建通过。
- Linux ASan / UBSan CI 已配置但本轮未执行；Windows 内存采样与热路径分配检查已执行通过。

历史文档迁移和旧实现证据不作为当前产品 PASS 的依据。

## 2026-09-20 host CI 夹具修复（本地验证）

- 失败基线：`1b59cfb48ca4cb4673c9ae3f683e4ef1fee4cf9a`，GitHub [run 35501009060](https://github.com/KnOFCA/HRK/actions/runs/35501009060)。配置、构建通过；CTest 报告 79 PASS / 12 FAIL，CLI 报 `ReplayChartMismatch`。此前 nothrow 分配回归已经 PASS。
- 原因：`simple_sequence.json` 的 Git 索引为 100 字节 LF，FNV-1a 为 `0da1f5ef330089a7`；工作区 CRLF 为 101 字节，哈希 `2ab45073a9e12c4c`，与 `mixed.replay` 头相同。Linux 检出 LF 破坏了既有夹具的字节绑定。
- 修复：`.gitattributes` 对测试谱面声明 `text eol=crlf`，保留既有 Replay 和原始字节哈希契约。
- 回归：用 `git -c core.autocrlf=false/true checkout-index --force --prefix=build/ci-checkout-<mode>/ -- tests/data/charts/simple_sequence.json tests/data/replay/mixed.replay` 分别检出；Node 计算 FNV-1a 并与 Replay 头对比，两者 PASS。两份检出输入调用 `build/host/Debug/hrk_headless.exe`，完整 JSON 与 `tests/expected/mixed_result.json` 相等。另用 `git show HEAD:tests/data/charts/simple_sequence.json` 复现原 LF 哈希不匹配。
- `cmake --build build/host --config Debug -j 8`：PASS；`ctest --test-dir build/host -C Debug --output-on-failure`：3/3 PASS。
- Git Bash 补齐 `/usr/bin:/bin` 的 PATH 后执行 `bash tools/sdd/check.sh`：PASS，123 链接、22 验证器测试、43 必需输入，FAIL=0 / BLOCKED=0。首次调用因本机 PATH 缺失 `dirname` 失败，修正调用环境后通过，未修改检查脚本。
- 本条为未提交工作区的本地修复验证，不覆盖既有正式产品验收证据。远程 Linux ASan/UBSan 尚未重跑，不声明远程 PASS。
