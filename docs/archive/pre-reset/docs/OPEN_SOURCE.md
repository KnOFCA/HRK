# 开源实践与第三方记录

本迭代未复制其他音游的判定或 Runtime 源码。SongClock、ClockMapper、TimingMap、输入管线、Session、Replay schema、接口与 ReferenceGame 均由 HRK 自行实现。

## 架构参考（不作为运行依赖）

- [osu!framework — InterpolatingFramedClock](https://github.com/ppy/osu-framework/blob/master/osu.Framework/Timing/InterpolatingFramedClock.cs)：参考音频源时钟与帧时钟的分层、暂停/seek 的独立处理；HRK v0.1.0 使用整数纳秒和音频源，不引入其插值平滑算法或 .NET Runtime。
- [StepMania — TimingData](https://github.com/stepmania/stepmania/blob/5_1-new/src/TimingData.cpp)：参考 Tempo 分段与 Beat/Time 双向查询；HRK 只实现初始 BPM 与 BPM change，自有数据模型与二分查询。
- [OpenHarmony — OHAudio playback](https://github.com/openharmony/docs/blob/master/en/application-dev/media/audio/using-ohaudio-for-playback.md)：参考 builder / renderer / callback 设备输出方式，实际接口还与本机 SDK 头文件核对。
- [OpenHarmony — Native TouchEvent](https://github.com/openharmony/docs/blob/master/en/application-dev/reference/apis-arkui/capi-oh-nativexcomponent-native-xcomponent-oh-nativexcomponent-touchevent.md)：按文档定义保留启动以来的纳秒 timestamp，而非回调到达时间。

这些参考解释设计取舍，不将上游实现视为本项目已验证行为的证据。

## 随源码分发的依赖

|依赖|固定版本|许可证|用途|
|---|---|---|---|
|[miniaudio](https://github.com/mackron/miniaudio/tree/0.11.23)|0.11.23|Public Domain / MIT-0 双许可；附原 LICENSE|预解码、PCM 格式转换、重采样；设备层编译禁用|
|[nlohmann/json](https://github.com/nlohmann/json/tree/v3.12.0)|3.12.0|MIT；附 LICENSE.MIT|ReferenceGame 的严格 JSON 解析；私有 include|

文件位于 `core/third_party/`，许可证随文件保留。构建不会下载浮动版本；`out/validation/report.json` 记录当前第三方文件的 SHA-256，供重现和审核。

测试 WAV 由 `tools/generate_fixtures.py` 数学生成为原创正弦波和节拍声，没有引入商业音乐。谱面、Replay、预期输出也由该脚本固定生成。
