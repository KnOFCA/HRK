# Harmony Rhythm Kernel · v0.1.0

面向 HarmonyOS Native 的 C++17 音游运行时原型。需求来自 `require_describe/v0.1.0/`。

本次实现包含 Kernel、独立 Game/Platform API、ReferenceGame、Headless、OHAudio/XComponent/GLES Backend、ArkTS Demo 和自动化验收。**当前是待设备验收的迭代实现，尚不能标记为正式 v0.1.0 Release**：本机没有连接 HDC 设备，真实音频、触摸、Surface 生命周期和设备 Replay → Headless 的 P0 证据仍待补齐。

## 构建与测试

主机只需要 CMake、C++17 编译器和 Python 3，不需要 Harmony SDK、GPU、音频设备或网络下载。

```powershell
python tools/validate.py --config Debug
```

也可以直接使用 CMake：

```powershell
cmake -S . -B out/host -DBUILD_TESTING=ON
cmake --build out/host --config Debug
ctest --test-dir out/host -C Debug --output-on-failure
```

Windows 多配置构建的命令行 Demo：

```powershell
out/host/core/Debug/hrk_headless.exe core/tests/data/charts/simple_sequence.json core/tests/data/audio/short_test.wav core/tests/data/replay/mixed.replay --json
```

结果为 `PERFECT, PERFECT, MISS, PERFECT`，总分 `300`。省略 Replay 文件使用 AutoPlay；`--save <路径>` 保存 Replay。Linux 单配置可执行文件位于 `out/host/core/hrk_headless`。

## HarmonyOS Demo

用 DevEco Studio 打开 `app/harmony`。工程面向 API 12+，当前用本机 SDK 6.1.1 / API 24 构建 arm64-v8a。独立原生入口是 `libhrk_entry.so`。

```powershell
python tools/build_harmony.py --deveco "D:/ophm/DevEco Studio"
# 或一次完成主机测试、Harmony 构建和 HAP 内容检查
python tools/validate.py --deveco "D:/ophm/DevEco Studio"
```

工具不使用其他项目的签名材料。默认生成：

`app/harmony/entry/build/default/outputs/default/entry-default-unsigned.hap`

真机安装前在 DevEco 中为 `com.hrk.prototype` 配置自己的开发签名。

页面操作顺序：加载 → 开始/自动/回放 → 暂停/继续/50%/停止。加载后默认“回放”使用固定 mixed Replay；完成游玩后“保存 Replay”写入应用 filesDir 下的 `last.replay` 和 `last_result.json`；“读取录制”可切换到该 Replay。重新运行前需结束当前 Session。返回前台保持暂停，需手动继续。

## 项目结构

```text
core/src/kernel/           时间、BPM、音频解码、输入、Replay、渲染与运行时
core/src/interface/        Game、Platform、Render 公共接口
core/src/games/reference/  唯一的 ReferenceGame 实现
core/src/platform/        Headless 与 Harmony 适配
core/src/app/             两个平台的 Composition Root
core/tests/               单元/集成测试、固定谱面、音频、Replay 和预期结果
app/harmony/              可构建的 ArkTS Demo 工程
tools/                    构建、架构扫描、Replay 比对和验收报告
docs/                     实现约定、开源参考、验收状态与设备检查步骤
```

所有 Harmony 原生 API 的调用和类型都位于 `core/src/platform/harmony/`；两个 Composition Root 使用相同 `hrk_runtime` 与 `hrk_game_reference`。第三方头文件不会进入公共接口。

详见 [实现与确定性约定](docs/ARCHITECTURE.md)、[开源参考与许可证](docs/OPEN_SOURCE.md)、[验收记录](docs/VALIDATION_v0.1.0.md)、[设备验收步骤](docs/DEVICE_ACCEPTANCE.md)。
