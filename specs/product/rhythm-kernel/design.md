# HRK v0.1.0 功能设计

依据 [spec](spec.md) 的需求与契约。2026-09-19 实施审查通过，按下列模块重新实现。
通用源码位于 src/kernel、src/interface、src/games、src/platform，应用组合位于 src/app，产品测试位于 tests。
序列化、输入容量及判定规则引用 spec 的实现前审查条款。

## 实施决策

输入分发余量遵循 spec 的运行时契约：仅推迟 Play 超时水位，不扩大命中窗口。帧率默认固定请求 60 Hz，120 Hz 为显式请求；基线资源预先加载，等待准备事件完成后同时启动音频与 60 秒采样。Harmony HAP 包含 arm64-v8a 与 x86_64，分别服务真机与模拟器。

- 公共类型和显式错误枚举置于 base；Game API 只依赖 base/render API，Platform API 只依赖 base/render API。Composition Root 是唯一同时认识具体游戏和平台的位置。
- 时间轴预编译 tempo 前缀时间。ReferenceGame 的 immutable chart 保存已排序对象；rules 维护判定状态；输入依时间推进 timeout scheduler，结果通过通用 judgment sink 分发到 score 和 diagnostics。
- Runtime 独占游戏状态；输入回调用 SPSC 固定队列，音频回调使用不可变 PCM 和原子游标。Harmony 控制路径串行修改状态，绘制回调使用 try_lock，繁忙时跳过该帧而不阻塞；音频回调不取此锁。EGL 每帧解除线程绑定，Surface 失去时暂停，再创建时重建 GPU 资源。输入回调仅入队，不读取游戏状态。
- 为满足最低 WAV 契约采用自有边界校验 PCM16 decoder，不新增第三方依赖；miniaudio 为允许而非强制依赖。文件解析仅在加载阶段执行。
- Replay 用显式小端逐字段编码，不序列化 C++ struct padding。所有版本、长度和数值检查在运行前完成。
- 主机构建使用 CMake/CTest；Harmony 用 SDK Native 交叉编译和 Hvigor HAP 构建。两者链接同一个 hrk_game_reference target。

# 4. 总体架构

```text
                   ArkTS Application
                          │
                         NAPI
                          │
                          ▼
                Harmony Composition Root
                          │
             ┌────────────┼────────────┐
             ▼            ▼            ▼
        HRK Runtime   Game Module   Harmony Backend
             │            │            │
             │            │            │
             ▼            ▼            ▼
          Kernel       Game API    Platform API
```

代码结构：

```text
src/
├─ kernel/
│  ├─ base/
│  ├─ time/
│  ├─ audio/
│  ├─ input/
│  ├─ timeline/
│  ├─ gameplay/
│  ├─ replay/
│  ├─ render/
│  ├─ resource/
│  └─ diagnostics/
│
├─ interface/
│  ├─ game/
│  └─ platform/
│
├─ platform/
│  ├─ harmony/
│  └─ headless/
│
├─ games/
│  └─ reference/
│
├─ app/
│  └─ harmony/
│
└─ third_party/
   └─ miniaudio/
```

---


# 23. Target 类型

建议：

```text
INTERFACE:
    hrk_platform_api
    hrk_game_api
    hrk_render_api

STATIC:
    hrk_base
    hrk_time
    hrk_timeline
    hrk_audio
    hrk_input
    hrk_render
    hrk_replay
    hrk_runtime
    hrk_platform_harmony
    hrk_platform_headless
    hrk_game_reference

SHARED:
    hrk_entry
```

HarmonyOS 最终只暴露：

```text
libhrk_entry.so
```

给 ArkTS/NAPI。

---


# 24. Target 依赖关系

标准依赖方向：

```text
                        miniaudio
                            │
                            ▼
                         audio

base ───────► time
 │
 ├──────────► timeline
 │
 ├──────────► replay
 │
 ├──────────► render_api ─────► render
 │
 ├──────────► platform_api ◄── harmony
 │                         ◄── headless
 │
 └──────────► game_api ◄────── reference_game


time ─────────────► input

game_api
time
timeline
audio
input
render
replay
platform_api
      │
      ▼
   runtime


runtime
harmony
reference_game
      │
      ▼
    entry
```

---


# 29. HarmonyOS 最小原型画面

v0.1.0 Harmony Demo 只需显示：

```text
--------------------------------

Harmony Rhythm Kernel

Song Time: 12.421 s

●       ●       ●

PERFECT: 2
MISS:    1

FPS: 120
Audio: Running

--------------------------------
```

其中 Note 以简单矩形或圆形表示即可。

不要求视觉接近任何现有音游。

---


# 30. v0.1.0 最小运行流程

启动：

```text
ArkTS Page
   ↓
Create XComponent
   ↓
NAPI Init
   ↓
Create HarmonyPlatform
   ↓
Create ReferenceGame
   ↓
Load Reference Chart
   ↓
Create GameplaySession
   ↓
Load Audio
   ↓
Ready
```

开始：

```text
Play
 ↓
OHAudio Start
 ↓
SongClock Start
 ↓
ReferenceGame Start
```

运行：

```text
Touch
 ↓
HarmonyInputBackend
 ↓
Input timestamp mapping
 ↓
IGameInput
 ↓
IGameRules
 ↓
Judgment
 ↓
Score
 ↓
Replay Recorder
```

渲染：

```text
Frame Callback
 ↓
Renderer
 ↓
ReferenceGameRenderer
 ↓
Render Commands
 ↓
GLES Backend
 ↓
XComponent
```

---


# 39. v0.1.0 最终架构

```text
                         Game Layer

                    ReferenceGame
                          │
                          ▼
                     Game API


                          │
                          ▼


                 ┌─────────────────┐
                 │   HRK Runtime   │
                 ├─────────────────┤
                 │ GameplaySession │
                 │ SongClock       │
                 │ Input           │
                 │ TimingMap       │
                 │ Audio           │
                 │ Replay          │
                 │ Render          │
                 └─────────────────┘

                          │
                          ▼

                    Platform API

                 ┌────────┴────────┐
                 ▼                 ▼

          Harmony Backend     Headless Backend

          OHAudio             FakeAudio
          XComponent          FakeInput
          NativeWindow        FakeClock
          EGL/GLES            NullRenderer
          Touch
```

真正的目标不是：

```text
做出一个小游戏
```

而是验证：

```text
           Game
            │
        Game Interface
            │
            ▼
       Rhythm Kernel
            │
      Platform Interface
            │
       ┌────┴─────┐
       ▼          ▼
   Harmony     Headless
```

这一依赖方向能够稳定成立。

---
