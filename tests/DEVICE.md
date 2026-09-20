# Harmony 设备验收执行步骤

预期与门禁引用 [验收契约](../specs/product/rhythm-kernel/acceptance.md)。任何步骤未实测不得记 PASS。HDC 安装即使进程返回 0，也必须检查其输出中的安装结果。

1. 构建签名 HAP，记录 SHA-256；连接设备，记录型号、系统与刷新率。安装 `entry-default-signed.hap`，启动 `com.hrk.reference / EntryAbility`。
2. 确认页面、Native Surface、清屏与移动音符；AutoPlay → 开始，观察音频、SongTime、PERFECT=4 / MISS=0 和 Score=400。
3. Replay → 开始，等待结束，确认 fixed mixed 的完整结果；保存 Replay 并回传 files/session-result.json，与 tests/expected/mixed_result.json 对比。
4. Play → 开始，在音符到达横线时触摸。验证 DOWN / MOVE / UP、多指及退出触摸的行为；记录实际触摸，不能用自动输入代替。保存 Replay，回传并用 hrk_headless 重放，比较完整结果 JSON。
5. 运行到 5 秒暂停，等待 2 秒后观察冻结，恢复 1 秒后确认约 6 秒；seek 50% 后确认位置与重建状态。切后台再回前台，确认保持 Paused；旋转或销毁/重建 Surface，确认安全释放与恢复。
6. 点击“60s 基线”，使用 64 秒音频与持续音符运行 AutoPlay；资源准备 3 秒后开始播放和采样，至少 67 秒后点击保存，回传 frame-metrics.json 检查 complete=true、帧间隔 p95 和最低每秒 FPS；支持时请求 120 Hz 并记录实际结果。不以瞬时 HUD FPS 代替完整采样。
7. 对剩余设备相关 TC 逐条记录实际操作、输入和结果，保留独立设备证据。主机 stress 测试不替代真机稳定性。

可用命令（从仓库根目录运行；hdc 必须在 PATH 或使用 SDK 完整路径）：

```powershell
hdc list targets
hdc install src/app/harmony/entry/build/default/outputs/default/entry-default-signed.hap
hdc shell aa start -b com.hrk.reference -a EntryAbility
hdc file recv -b com.hrk.reference /data/storage/el2/base/haps/entry/files/session.replay build/device/session.replay
hdc file recv -b com.hrk.reference /data/storage/el2/base/haps/entry/files/session-result.json build/device/session-result.json
build/host/Debug/hrk_headless.exe tests/data/charts/simple_sequence.json tests/data/audio/short_test.wav build/device/session.replay
```

资源加载失败不进入 Playing；Backend 错误应显示可追溯错误。无签名、设备断开、接口不可用均记录 BLOCKED，不放宽 Release Gate。

自动化平台输入与生命周期：`python tools/test-device-input.py --hdc <hdc路径>`、`python tools/test-device-lifecycle.py --hdc <hdc路径>`。平台注入不替代真实手指与听感确认。模拟器性能测量期间暂停主机构建、全盘搜索等负载；保留失败探测与最终独立运行两类结果。
