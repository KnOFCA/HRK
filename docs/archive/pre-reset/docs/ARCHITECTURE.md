# v0.1.0 实现与确定性约定

## 依赖与所有权

`hrk_base` 不依赖其他 HRK target。公共接口分别为 `hrk_game_api`、`hrk_platform_api`、`hrk_render_api`；模块以各自的 include 目录向下游导出头文件，采用 target-based CMake。

`hrk_time` 依赖 Platform API；`hrk_timeline` 只依赖 base；`hrk_input` 依赖 time；`hrk_audio` 私有依赖 miniaudio。`hrk_runtime` 编排这些模块及 Game API，不包含 ReferenceGame 或 Harmony 类型。`hrk_game_reference` 私有依赖 timeline 和 JSON 解析器，无 Platform 依赖。

`core/src/app/headless/main.cpp` 和 `core/src/app/harmony/entry.cpp` 是组合点。Harmony 组合点把 `IApplication` 工厂交给平台桥接，NAPI/XComponent 的注册、资源读取、生命周期和原生句柄全部留在平台目录。

Harmony Demo 当前支持单个 XComponent/Session。NAPI、Surface 与帧回调由平台桥接的互斥锁串行进入 Session；音频线程不获取此锁，也不调用 Gameplay。EGL 每次提交后解除当前 Context，使串行调用可在不同回调线程上安全接管。

## 时间与输入

核心时间为有符号 `int64_t` 纳秒；边界使用 `HostTime`、`AudioTime`、`SongTime`、`PresentationTime` 区分域。正式 SongClock 读取音频位置，不累加渲染 delta。

Headless 人工推进 Host 和音频时钟；暂停只推进 Host。Harmony 使用 `CLOCK_MONOTONIC` 的 OHAudio 硬件呈现 timestamp/frames，投影至当前 Host，受已提交 PCM 及音频长度上限约束。尚未取得有效硬件时间戳时返回 `advancing=false`，游戏等待，不用帧时间伪造音频时间。当前不支持变速，采用 API 10 的 GetTimestamp；回调注册使用 API 12 接口。

音频预先解码为 48 kHz、双声道 float PCM。回调只有拷贝、补零和原子游标操作。暂停、seek、resume 通过停止/释放并重建 renderer，从记录的歌曲位置重新提交 PCM，避免使用不明确的 flush 后硬件帧原点。该原型方案可能有重建延迟，须在设备上检查；不宣称无缝 seek 或成熟延迟校准。

原生 Touch DOWN/UP 使用事件的 id/坐标/timeStamp，MOVE/CANCEL 遍历触点。坐标归一化到 `[0,1]`；timeStamp 按平台文档保留单调时钟纳秒值，不重读 handler 到达时间。队列为容量 2048 的 SPSC 固定环形队列；满时丢弃最新事件、增加计数，并暂停运行报错。触点活动状态使用 64 个固定槽，支持独立 id 和 CANCEL。

输入按歌曲时间稳定排序，同 timestamp 保留到达顺序。运行期间，落在已处理时间之前的合法事件会触发从已录制输入重建 Rules/Score/Judgment log。已入队输入不会因为某个渲染帧耗时过长而被丢弃；每次结束检查前先清算输入队列。最终结果是权威结果，迟到修正期间 HUD 可能回滚一次。过旧生命周期、非法时间和坐标事件明确计入 rejected；有 rejected/dropped 的设备运行不允许作为验收 PASS。

Play 的结束在最后判定截止后额外容纳 100 ms（不超过音频长度），以接收最后一帧附近的迟到输入；已经进入 Finished 后到达的回调不再改变结果。Replay/AutoPlay 无需等待该窗口。暂停/恢复/seek 会清空待处理输入及触点，并重设 Host epoch；旧 epoch 事件不会进入新一轮判定。

## ReferenceGame

JSON 格式：

```json
{
  "bpm": 120,
  "tempos": [{"beat": 4, "bpm": 240}],
  "notes": [{"beat": 1, "x": 0.25}, {"beat": 5, "x": 0.5}]
}
```

`tempos` 可省略；初始 BPM 对应 beat 0，其余 tempo 严格按 beat 递增。加载器检查 BPM、位置、数据形状、大小与时长，编译成不可变 RuntimeChart。Note id 从 1 开始按原始文件顺序赋值；相同时刻稳定排序。最多 100,000 个对象、600 秒谱面，测试音频需覆盖最后窗口。

TimingMap 预计算分段起始纳秒，用二分查找双向转换。Beat/BPM 使用 IEEE binary64，中间计算保持相同运算顺序，转换到 TimeNs 时 `llround`；避免依赖 Windows/ARM 精度不同的 long double。未来 Stop/Warp 应扩展事件模型，不复用平台时钟。

规则版本为 1：只有 DOWN 参与判定，`|error| <= 50 ms` 且 `|x-note.x| <= 0.10f` 为 PERFECT，+100 分；未命中则 MISS，+0 分。多个候选按谱面稳定顺序选最早一个。一个对象只产生一次判定。

MISS 的确定时间固定为 `note.time + 50 ms + 1 ns`，其中 `inputTime` 是该超时截止，不是触摸时间；`error` 固定为 `50,000,001 ns`。因此即使一个 tick 跳过整首歌，也不改变判定时间和先后顺序。IGameRules 自身不读取 Host/Render/GPU。

## Replay 与 seek

文件格式 v1：小端，固定 IEEE 754 binary32 坐标；无原始 C++ struct 内存拷贝或平台相关 padding。

|字段|字节数|
|---|---:|
|magic `HRKR`|4|
|formatVersion|4|
|gameId|8|
|chartHash|8|
|ruleVersion|4|
|eventCount|4|
|每事件 songTime / pointerId / phase / x / y|8 / 4 / 1 / 4 / 4|

Phase：0=DOWN，1=MOVE，2=UP，3=CANCEL。最多 1,000,000 条事件、事件时间 0..86400 秒；拒绝未知版本、未知 phase、非有限坐标、非有序时间、截断和尾部多余字节。运行前校验 gameId、ruleVersion 和 chartHash。

chartHash 为原始 UTF-8 文件字节的 FNV-1a 64 位值，不是 `std::hash`。修改空白也会改变 hash；跨 Backend 必须使用完全相同的谱面文件。测试资源以二进制读取，避免 CRLF 文本模式改变内容。

seek 会重建从 0 到目标时间的确定性状态。Replay/AutoPlay 保留全部输入；Play 只保留目标及以前的录制输入，丢弃目标之后的旧历史，随后继续录制。跳过但未命中的对象按原时间截止为 MISS，所有得分由输入前缀重建。半途 stop 的未完成谱面不允许导出为完整验收结果。

Replay 仅保存歌曲时间域输入，不保存 GPU、帧调度或平台句柄。事件处理先推进到每个输入时间，派发输入，再推进到当前音频时间。所有 JudgmentResult 字段和总分均参与比较，不能只比较 Score。

## 扩展边界

miniaudio 关闭设备层、Engine、ResourceManager、NodeGraph，仅作解码/转换/重采样。ReferenceGame Renderer 只提交 Clear/Sprite（无纹理时为 Quad）；GLES Backend 支持 RGBA 纹理，并随 Surface 销毁释放 Context 资源。当前 Game 使用无纹理色块，所以 Surface 重建无需重新加载 Game 资源。

IFileSystem 提供明确 ResourceNotFound；Harmony 包内文件由 Native ResourceManager 读取，再传给组合点。Diagnostics 记录 frames/FPS、输入、拒绝/丢失和 Judgment 总数；具体得分种类由 Game 的 ScoreSystem 维护。
