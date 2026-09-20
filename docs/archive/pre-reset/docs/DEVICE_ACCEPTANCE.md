# v0.1.0 设备验收

主机测试和 HAP 编译不能证明 OHAudio 硬件呈现、真实触摸或 Surface 重建正确。本轮 HDC 返回 `[Empty]`，下列设备项目保持 NOT_RUN。

## 准备

1. DevEco 打开 `app/harmony`，为 `com.hrk.prototype` 配置个人开发签名。
2. 连接支持 API 12+、XComponent/OHAudio 的 HarmonyOS 设备；记录型号、OS/API、屏幕刷新率、音频路由与构建 commit/源码摘要。
3. 构建安装测试包。不要用命令行注入点击代替真实 Touch Timestamp 和多指检查。
4. 主机先执行 `python tools/validate.py --deveco "<DevEco 安装目录>"`，保留 `out/validation/` 的日志、hash 和 CTest XML。

## P0 与生命周期检查

|项目|操作与预期|当前状态|
|---|---|---|
|TC-HAR-001..004|Surface 创建、清屏、下落矩形、窗口 resize 均正常|NOT_RUN|
|TC-HAR-IN-001..005|真实单指/多指 DOWN、MOVE、UP；pointer id 独立，原始时间戳来自事件；HUD 无 rejected/dropped|NOT_RUN|
|TC-HAR-AUD-001..005|加载后有节拍声，Audio Running，SongTime 推进；暂停停止推进，恢复连续，50% 跳转到约 1 秒|NOT_RUN|
|TC-E2E-001..003|真实触摸指定区域生成 PERFECT；不触摸生成 MISS；HUD 和得分一致|NOT_RUN|
|TC-E2E-004..005|保存真实游玩 Replay 和结果，在 Headless 完整复现每条判定及得分|NOT_RUN|
|TC-BACKEND-001..003|同一文件、同一个游戏库；设备固定 mixed Replay 为 P/P/M/P 和 300 分|NOT_RUN|
|TC-LIFE-001..004|切后台自动暂停；前台不跳时；Surface 销毁/重建无 crash，重新创建后显示正常|NOT_RUN|
|TC-HAR-FRAME-001..003|帧回调持续；60/120 Hz 设备上固定 Replay 一致|NOT_RUN|
|TC-PERF-001..002|连续运行/重开 60 秒，观察 60 Hz 稳定性；120 Hz 仅在设备支持时记录 PASS，否则 UNSUPPORTED_DEVICE|NOT_RUN|
|TC-STAB / TC-MEM|真机反复开始停止、暂停恢复、seek；观察内存与声音回调无持续增长或卡死|NOT_RUN|

当前固定音频 4 秒、谱面目标时间 0.5/1.0/1.5/2.0 秒。较长暂停可在 1 秒附近进行，暂停 2 秒后继续再检查歌曲时间，不需要长音轨。

## 固定 Replay 验收

加载后点击“回放”，应得到：

|对象|类型|targetTime|inputTime|error|
|---|---|---:|---:|---:|
|1|PERFECT|500000000|500000000|0|
|2|PERFECT|1000000000|1020000000|20000000|
|3|MISS|1500000000|1550000001|50000001|
|4|PERFECT|2000000000|1960000000|-40000000|

全部时间为纳秒。得分 300、Perfect 3、Miss 1。MISS 的 inputTime 是规则超时事件的时间。

## 真实录制 → Headless

1. 加载 → 开始 → 用真实手指完成谱面 → 保存 Replay。
2. 从本应用的 filesDir 导出 `last.replay` 和 `last_result.json` 到工作区的独立验收目录，保留原始文件。可使用 DevEco 文件浏览器；设备和开发签名允许时也可使用 HDC 文件导出。
3. 使用包内同一谱面文件和测试音频执行：

```powershell
python tools/verify_replay.py out/host/core/Debug/hrk_headless.exe core/tests/data/charts/simple_sequence.json core/tests/data/audio/short_test.wav out/device/last.replay out/device/last_result.json
```

脚本检查全部 Judgment 字段、顺序和 Score，并验证 Replay 的实际磁盘保存/加载字节一致；设备结果如包含 rejected/dropped 非零，即使得分相同也返回失败。

4. 在设备点击“读取录制” → “回放”，再次保存到另一个验收目录并比较结果；不要覆盖首份真实录制证据。
5. 保存设备日志、录屏/截图和两份原始结果，逐项填写设备矩阵。只有实际执行的项目才能写 PASS；缺少设备证据时不允许发布 v0.1.0。
