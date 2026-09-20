# Harmony Rhythm Kernel v0.1.0 最小迭代原型规格说明书

版本：v0.1.0  
项目名称：Harmony Rhythm Kernel  
简称：HRK  
目标平台：HarmonyOS Native  
核心语言：C++17  
构建系统：CMake  
主要运行模式：HarmonyOS Native / Headless Test

---

# 1. 项目定位

Harmony Rhythm Kernel 是一个面向移动端音游的通用原生运行时内核。

其目标不是提供通用游戏引擎，而是提供音游所共同需要的底层能力：

- 高精度歌曲时钟；
- 音频驱动时间轴；
- 带时间戳的输入处理；
- Beat / Time 时间轴转换；
- 游戏运行生命周期；
- 判定事件分发；
- Replay；
- 渲染抽象；
- HarmonyOS 平台适配；
- Headless 确定性测试。

具体游戏规则，例如：

- PJSK 的 Flick / Slide / Trace；
- Arcaea 的 Arc / Arctap；
- Phigros 的 JudgeLine；
- Mania 的 Lane / Long Note；

均不属于 HRK Kernel，而通过 Game Interface 独立实现。

系统总体原则为：

```text
Platform != Kernel != Game

Platform
   ↓
Platform Interface
   ↓
Kernel Runtime
   ↑
Game Interface
   ↑
Game Module
```

v0.1.0 的首要目标是验证这一架构是否成立，而不是实现完整游戏。

---

# 2. v0.1.0 核心目标

本版本必须证明以下四件事情。

## 2.1 Kernel 与 HarmonyOS 解耦

Kernel 中不得直接出现：

```text
OHAudio
XComponent
OHNativeWindow
EGL
OpenGL ES
NAPI
ArkTS
```

所有 HarmonyOS API 必须限制在：

```text
platform/harmony/
```

目录。

---

## 2.2 Kernel 与具体游戏解耦

Kernel 中不得出现：

```text
PJSK
Arcaea
Phigros
Mania
Tap
Flick
Slide
Arc
JudgeLine
Lane
```

等具体玩法概念。

Kernel 只处理：

```text
Time
Input
Timeline
Audio
Render Command
Runtime
Replay
Event
Resource
```

---

## 2.3 同一游戏可运行于两个 Backend

v0.1.0 必须实现一个最小：

```text
ReferenceGame
```

并保证其代码无需修改，即可分别运行于：

```text
ReferenceGame
      │
      ├── HeadlessBackend
      │
      └── HarmonyBackend
```

---

## 2.4 Headless 运行具有确定性

对于：

```text
相同 Chart
+
相同 Replay Input
+
相同 Rule Version
```

必须得到：

```text
完全相同的 Judgment Sequence
完全相同的 Score Result
```

与：

```text
FPS
设备
渲染状态
```

无关。

这是 HRK v0.1.0 最核心的架构验收目标。

---

# 3. 非目标

v0.1.0 明确不实现：

- PJSK；
- Arcaea；
- Phigros；
- 完整 Mania；
- 通用 ECS；
- Scene Graph；
- 物理系统；
- 动画系统；
- 网络系统；
- Lua/JS 脚本系统；
- Sonolus Runtime；
- Vulkan Backend；
- PC SDL Backend；
- 完整资源包系统；
- 谱面编辑器；
- 在线服务器；
- MV；
- 复杂 Shader；
- 完整 UI Framework；
- 正式皮肤系统。

本版本仅构建未来这些模块所依赖的最小 Kernel。

---

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

# 5. 模块职责

## 5.1 `kernel/base`

负责整个 HRK 的最底层公共类型。

允许包含：

```text
TimeNs
DurationNs
Beat
Vec2
Vec3
Mat4

EntityId
ResourceId
GameId

Result
RingBuffer
FixedQueue
```

建议时间统一使用：

```cpp
using TimeNs = int64_t;
```

禁止在核心实时接口中使用：

```cpp
float milliseconds;
double currentTime;
```

作为主时间表示。

允许提供：

```cpp
double seconds(TimeNs);
double milliseconds(TimeNs);
```

作为转换接口。

`base` 不允许依赖任何其他 HRK target。

---

# 6. `kernel/time`

负责运行时高精度时间体系。

至少实现：

```text
MonotonicClock
AudioClock
SongClock
ClockMapper
```

核心时间概念必须分离：

```text
HostTime
AudioTime
SongTime
PresentationTime
```

不得使用：

```text
songTime += frameDelta
```

作为正式 SongClock。

歌曲时间原则上由音频播放位置驱动。

输入事件通过：

```text
Host Timestamp
      ↓
ClockMapper
      ↓
Song Timestamp
```

映射到歌曲时间。

v0.1.0 不要求实现复杂 ClockSmoother，但必须保留扩展能力。

---

# 7. `kernel/timeline`

负责纯粹的音乐时间映射。

至少支持：

```text
Beat
BPM
Beat → Time
Time → Beat
```

数据结构最低要求：

```cpp
struct TempoPoint {
    Beat beat;
    double bpm;
};
```

接口示例：

```cpp
class TimingMap {
public:
    TimeNs beatToTime(Beat beat) const;
    Beat timeToBeat(TimeNs time) const;
};
```

v0.1.0 仅要求：

- 初始 BPM；
- 多 BPM Event；
- Beat-Time 双向转换。

暂不要求：

- Stop；
- Warp；
- Delay；
- Scroll Speed；
- Time Signature。

但数据模型不得妨碍未来增加这些事件。

`timeline` 必须是纯计算模块，不依赖 Audio、Input 或 Platform。

---

# 8. `kernel/audio`

HRK Audio 分成两层：

```text
Codec / DSP
     ↓
HRK Audio Runtime
     ↓
IAudioBackend
     ↓
Platform Audio Device
```

v0.1.0 允许直接依赖：

```text
miniaudio
```

但使用范围限制为：

- 音频解码；
- PCM 格式转换；
- 重采样。

禁止将 miniaudio 设备层作为 HarmonyOS 正式 Backend。

HarmonyOS 输出必须使用：

```text
OHAudio
```

建议：

```text
miniaudio
   ↓
PCM
   ↓
HRK AudioMixer
   ↓
IAudioBackend
   ↓
OHAudioBackend
```

v0.1.0 最低要求：

- 加载一段测试音频；
- 解码成 PCM；
- 播放；
- pause；
- resume；
- seek；
- 查询播放位置。

---

# 9. `kernel/input`

负责将平台原始输入转换为 HRK 通用输入事件。

平台输入：

```text
Harmony Touch
      ↓
HarmonyInputBackend
      ↓
RawInputEvent
      ↓
InputQueue
      ↓
Timestamp Mapping
      ↓
InputEvent
```

公共事件至少包含：

```cpp
struct InputEvent {
    uint32_t pointerId;

    InputPhase phase;

    Vec2 position;

    TimeNs songTime;
};
```

其中：

```text
InputPhase =
DOWN
MOVE
UP
CANCEL
```

Kernel 不负责：

```text
lane mapping
flick detection
arc mapping
slide ownership
```

这些属于 Game Module。

---

# 10. `kernel/render`

渲染分两层：

```text
Game
 ↓
Render API
 ↓
HRK Render Runtime
 ↓
IRenderBackend
 ↓
OpenGL ES
```

Game 不得直接：

```cpp
#include <GLES3/gl3.h>
```

Game 只能使用类似：

```text
TextureHandle
MeshHandle
SpriteDrawCommand
MeshDrawCommand
IRenderContext
```

v0.1.0 仅实现：

- Clear；
- Sprite；
- 简单 Quad；
- 基础 texture；
- 简单 batching 可选。

不要求：

- FrameGraph；
- 后处理；
- 自定义 RenderPass；
- 复杂材质系统。

---

# 11. `kernel/gameplay`

Gameplay 是 Kernel orchestration 层。

核心对象：

```text
GameplaySession
GameRuntime
JudgmentDispatcher
```

其主要职责：

```text
start
pause
resume
seek
stop

process input
update game
dispatch judgment
record replay
render
```

核心生命周期：

```text
Created
  ↓
Loading
  ↓
Ready
  ↓
Playing
  ↓
Paused
  ↓
Finished
```

不得在 GameplaySession 中实现任何具体音游判定。

---

# 12. Game Interface

v0.1.0 最低接口集合：

```text
IGameModule
IChartLoader
IRuntimeChart
IGameRules
IGameRenderer
IGameInput
IScoreSystem
```

建议顶层入口：

```cpp
class IGameModule {
public:
    virtual ~IGameModule() = default;

    virtual GameDescriptor descriptor() const = 0;

    virtual std::unique_ptr<IChartLoader>
        createChartLoader() = 0;

    virtual std::unique_ptr<IGameRules>
        createRules() = 0;

    virtual std::unique_ptr<IGameRenderer>
        createRenderer() = 0;

    virtual std::unique_ptr<IGameInput>
        createInput() = 0;

    virtual std::unique_ptr<IScoreSystem>
        createScoreSystem() = 0;
};
```

Game Interface 不允许包含任何 HarmonyOS 类型。

---

# 13. Platform Interface

至少定义：

```text
IAudioBackend
IInputBackend
IRenderBackend
IRenderSurface
IFrameScheduler
IFileSystem
ILifecycleBackend
```

关系：

```text
Kernel
  ↓
Platform Interface
  ↑
Harmony Backend
```

而不是：

```text
Kernel
  ↓
HarmonyOS API
```

---

# 14. HarmonyOS Backend

目录：

```text
platform/harmony/
├─ audio/
│  └─ OHAudioBackend.cpp
├─ input/
│  └─ HarmonyInputBackend.cpp
├─ render/
│  ├─ HarmonyRenderSurface.cpp
│  └─ GlesRenderBackend.cpp
├─ HarmonyFrameScheduler.cpp
└─ HarmonyLifecycle.cpp
```

v0.1.0 使用：

```text
XComponent
OHNativeWindow
EGL
OpenGL ES
OHAudio
NAPI
```

其中：

```text
ArkTS
```

只负责：

- Activity/Page；
- XComponent；
- 基础生命周期；
- 启动/停止测试场景。

不实现 Gameplay。

---

# 15. Headless Backend

必须与 Harmony Backend 同期实现。

目录：

```text
platform/headless/
├─ FakeAudioBackend.cpp
├─ FakeInputBackend.cpp
├─ NullRenderBackend.cpp
└─ FakeFrameScheduler.cpp
```

Headless 用途：

```text
Unit Test
Gameplay Test
Replay Test
Timing Test
CI
```

FakeAudioBackend 必须允许：

```cpp
clock.advance(10_ms);
```

或类似方式人工推进时间。

这使测试不依赖真实设备时间。

---

# 16. ReferenceGame

v0.1.0 不直接开发 PJSK/Arcaea。

必须先实现一个最小 ReferenceGame。

它只包含一种游戏对象：

```text
SimpleNote
```

数据：

```cpp
struct SimpleNote {
    TimeNs time;
    float x;
};
```

玩法：

```text
在 Note 时间附近触摸屏幕指定区域
        ↓
判断 timing error
        ↓
Perfect / Miss
```

只需两种判定：

```text
PERFECT
MISS
```

例如：

```text
|error| <= 50 ms → PERFECT
其他 → MISS
```

ReferenceGame 的目的不是测试音游复杂度，而是验证：

```text
Chart
Clock
Input
Judgment
Score
Replay
Renderer
Platform
```

能够完整闭环。

---

# 17. Reference Chart

可以使用非常简单的文本格式：

```text
BPM 120

NOTE 1.0 0.25
NOTE 2.0 0.50
NOTE 3.0 0.75
```

或者 JSON：

```json
{
  "bpm": 120,
  "notes": [
    { "beat": 1, "x": 0.25 },
    { "beat": 2, "x": 0.50 },
    { "beat": 3, "x": 0.75 }
  ]
}
```

加载流程：

```text
Reference Chart File
       ↓
IChartLoader
       ↓
Mutable Model
       ↓
Compile
       ↓
ReferenceRuntimeChart
```

RuntimeChart 一旦开始游戏必须不可变。

---

# 18. Judgment Framework

Kernel 负责公共事件结构：

```cpp
struct JudgmentResult {
    EntityId entity;

    JudgmentId type;

    TimeNs targetTime;

    TimeNs inputTime;

    TimeNs error;
};
```

但：

```text
PERFECT
GREAT
GOOD
BAD
MISS
```

的具体定义属于 Game。

ReferenceGame 只定义：

```text
PERFECT
MISS
```

Kernel 只负责：

```text
dispatch
record
forward to score
forward to replay
```

---

# 19. Replay

v0.1.0 Replay 必须成为正式 Kernel 功能，而不是后续补充。

最低 Replay Input：

```cpp
struct ReplayInputEvent {
    TimeNs songTime;

    uint32_t pointerId;

    InputPhase phase;

    Vec2 position;
};
```

Replay Header：

```cpp
struct ReplayHeader {
    uint32_t formatVersion;

    GameId gameId;

    uint64_t chartHash;

    uint32_t ruleVersion;
};
```

v0.1.0 不要求复杂压缩。

可以采用简单：

```text
binary
```

或：

```text
JSON
```

格式。

但必须满足：

```text
record
save
load
playback
```

完整闭环。

---

# 20. Runtime Mode

至少定义：

```cpp
enum class RuntimeMode {
    Play,
    Replay,
    AutoPlay
};
```

v0.1.0：

```text
Play
```

必须工作。

```text
Replay
```

必须可用于 Headless deterministic test。

```text
AutoPlay
```

只需 ReferenceGame 最小支持。

---

# 21. 线程模型

v0.1.0 推荐逻辑模型：

```text
              UI Thread
                 │
               ArkTS
                 │
                 ▼
           Control / NAPI

Audio Callback              Input Callback
      │                          │
      ▼                          ▼
Audio Position            Timestamped Event
      │                          │
      └─────────┐     ┌──────────┘
                ▼     ▼
             Kernel Runtime
                  │
                  ▼
              Game State
                  │
                  ▼
               Render
```

必须遵守：

```text
Audio callback
```

不得执行：

- 文件 IO；
- Chart parsing；
- malloc-heavy 工作；
- 长时间锁；
- Gameplay update；
- Logging flood。

Input Event 必须保存事件原始时间戳。

判定不得使用：

```text
render frame time
```

替代输入 timestamp。

---

# 22. CMake Targets

v0.1.0 固定以下核心 targets：

```text
hrk_base

hrk_platform_api
hrk_game_api
hrk_render_api

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

hrk_entry
hrk_tests
```

第三方：

```text
hrk_miniaudio
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

# 25. 禁止依赖关系

必须明确禁止：

```text
kernel → games
kernel → platform/harmony

games → platform
games → OHAudio
games → GLES
games → XComponent

platform → games

base → 任何其他 HRK module
timeline → audio
timeline → input
time → input
```

只有：

```text
hrk_entry
```

即 Composition Root 可以同时依赖：

```text
Kernel
Game
Platform
```

---

# 26. CMake 依赖规则

禁止全局：

```cmake
include_directories(...)
link_libraries(...)
```

只允许 target-based：

```cmake
target_include_directories(...)
target_link_libraries(...)
target_compile_definitions(...)
```

统一语义：

```text
PUBLIC
公共 header 暴露该 dependency

PRIVATE
仅当前实现使用

INTERFACE
target 本身无实现，仅传播 API dependency
```

例如：

```cmake
target_link_libraries(hrk_audio
    PUBLIC
        hrk::base
        hrk::platform_api

    PRIVATE
        hrk_miniaudio
)
```

从而禁止 miniaudio 类型泄漏到 HRK Public API。

---

# 27. 第三方代码复用边界

## 27.1 Direct Dependency

v0.1.0：

```text
miniaudio
```

用途：

```text
decode
resample
format conversion
```

不得直接控制 HarmonyOS 音频设备。

---

## 27.2 Reference Only

以下项目只作为架构或行为参考：

### osu!framework

参考：

```text
Clock
Threading
Input scheduling
Render abstraction
Ruleset architecture
Replay architecture
```

不得成为 runtime dependency。

---

### StepMania

参考：

```text
TimingData
Beat ↔ Time
Tempo changes
```

v0.1.0 不直接引入其 Runtime。

---

### Etterna

参考：

```text
Timing
Judgment
Replay
Mania behavior
```

---

### Sonolus

参考：

```text
Runtime / Engine separation
Play / Watch model
Game-independent runtime
```

---

### Next-SEKAI

v0.1.0 不使用。

未来 PJSK Game Module 开发时作为：

```text
behavior oracle
```

---

# 28. HRK 自有核心代码

以下内容必须由 HRK 自己定义并长期维护：

```text
SongClock

ClockMapper

TimingMap representation

Input timestamp pipeline

GameplaySession

Game Interface

Platform Interface

Replay schema

Render API

Runtime mode

Determinism contract
```

任何第三方类型均不得直接成为这些公共接口的一部分。

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

# 31. 单元测试要求

v0.1.0 至少应包含以下测试。

## Time

```text
Monotonic ordering
Clock start
Pause
Resume
Seek
HostTime → SongTime mapping
```

---

## Timeline

```text
120 BPM:
Beat 0 → 0s
Beat 1 → 0.5s
Beat 2 → 1.0s

BPM Change:
Beat-Time conversion
Time-Beat inverse conversion
```

---

## Input

```text
DOWN
MOVE
UP

Pointer ID preserved

Host timestamp correctly mapped
```

---

## Gameplay

输入：

```text
Note @ 1000ms
Touch @ 1020ms
```

结果：

```text
PERFECT
error = +20ms
```

输入：

```text
Touch @ 1200ms
```

结果：

```text
MISS
```

---

## Replay

第一次：

```text
Chart A + Replay A
```

得到：

```text
P P M P
score = X
```

重复运行 100 次：

```text
结果必须完全一致
```

---

# 32. Headless 验收

必须能够运行：

```bash
hrk_tests
```

而无需：

```text
HarmonyOS device
XComponent
GPU
Audio device
```

最低验收：

```text
Timing tests PASS

Clock tests PASS

Replay tests PASS

Reference game tests PASS

Determinism tests PASS
```

---

# 33. HarmonyOS 验收

设备或模拟环境中最低要求：

1. ArkTS 页面正常加载；
2. Native `.so` 加载成功；
3. XComponent 正常创建；
4. OpenGL ES 成功清屏；
5. 测试 Note 正常绘制；
6. OHAudio 能播放测试音乐；
7. SongClock 随音频正常推进；
8. Touch DOWN/MOVE/UP 能进入 Native；
9. Touch timestamp 能映射到 SongTime；
10. ReferenceGame 能产生 PERFECT/MISS；
11. Score HUD 能更新；
12. Replay 能记录本次输入；
13. pause/resume 正常；
14. exit 不 crash。

---

# 34. 架构验收

以下检查比视觉效果更重要。

## 条件 A

以下目录搜索：

```text
src/kernel/
```

不得出现：

```text
ohaudio
xcomponent
native_window
GLES
pjsk
arcaea
phigros
```

---

## 条件 B

以下目录：

```text
src/games/
```

不得 include：

```text
src/platform/
```

---

## 条件 C

以下目录：

```text
src/platform/
```

不得 include：

```text
src/games/
```

---

## 条件 D

必须证明：

```text
hrk_game_reference
```

同时能与：

```text
hrk_platform_headless
```

和：

```text
hrk_platform_harmony
```

组合。

---

# 35. Performance 基线

v0.1.0 暂不做极限性能优化，但要求避免明显错误设计。

目标：

```text
Gameplay hot path:
无文件 IO
无 parser
无 blocking system call
```

Audio callback：

```text
无动态资源加载
无日志 flood
无复杂锁
```

Input 热路径：

```text
尽量无 heap allocation
```

渲染目标：

```text
60 FPS 必须稳定

120 FPS：
设备支持时作为目标
```

但判定结果不得依赖 60/120 FPS。

---

# 36. Determinism Contract

同一个：

```text
Kernel Version
Game Rule Version
Chart Hash
Replay
```

必须产生：

```text
相同 Judgment Count
相同 Judgment Order
相同 Timing Error
相同 Score
```

禁止 Gameplay Rule 直接依赖：

```text
Render FPS
system wall clock
random_device
GPU state
```

如果需要随机数，未来必须使用：

```text
deterministic seeded RNG
```

---

# 37. v0.1.0 推荐开发顺序

阶段 1：

```text
hrk_base
hrk_platform_api
hrk_game_api
hrk_render_api
```

完成所有公共接口和依赖规则。

---

阶段 2：

```text
hrk_time
hrk_timeline
```

首先建立精确时间体系。

---

阶段 3：

```text
hrk_platform_headless
hrk_game_reference
hrk_runtime
```

优先完成：

```text
Headless
       +
ReferenceGame
       +
Deterministic Test
```

此时就应有第一个 v0.1.0 milestone。

---

阶段 4：

```text
hrk_audio
```

接入：

```text
miniaudio decoder
+
FakeAudioBackend
```

然后实现：

```text
OHAudioBackend
```

---

阶段 5：

```text
hrk_input
hrk_replay
```

完成真实输入和 replay 闭环。

---

阶段 6：

```text
hrk_render
hrk_platform_harmony
```

完成：

```text
XComponent
NativeWindow
EGL
OpenGL ES
```

---

阶段 7：

```text
hrk_entry
```

实现 HarmonyOS Composition Root。

---

# 38. v0.1.0 Definition of Done

HRK v0.1.0 只有同时满足以下条件才视为完成：

- [ ] C++17 Kernel 可独立编译；
- [ ] 无 HarmonyOS 依赖进入 Kernel；
- [ ] Game API 与 Platform API 已分离；
- [ ] ReferenceGame 完成；
- [ ] Headless Backend 完成；
- [ ] Harmony Backend 最小实现完成；
- [ ] Audio Clock 驱动 SongClock；
- [ ] Input 使用事件 timestamp 判定；
- [ ] BPM/Beat-Time 转换完成；
- [ ] ReferenceGame 可以判定 PERFECT/MISS；
- [ ] Score 可以更新；
- [ ] Replay 可以 record/playback；
- [ ] 同一 Replay 多次运行结果一致；
- [ ] 同一个 ReferenceGame 可运行于 Headless 和 Harmony；
- [ ] HarmonyOS 可以播放一段测试音乐；
- [ ] HarmonyOS 可以渲染测试谱面；
- [ ] HarmonyOS 真输入可以产生判定；
- [ ] pause/resume/stop 不 crash；
- [ ] CMake dependency graph 不存在逆向依赖；
- [ ] CI 架构扫描通过；
- [ ] 所有核心测试通过。

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

# 40. v0.1.0 之后的演进路线

v0.1.0：

```text
HRK architecture proof
+
ReferenceGame
```

v0.1.1：

```text
更完整 AudioClock
Calibration
Diagnostics
Replay validation
```

v0.1.2：

```text
Mania Reference Ruleset
Hold Note
Multi-lane
```

v0.2.0：

```text
Arcaea Module migration
Continuous Touch
3D Playfield
Arc
```

v0.3.0：

```text
PJSK Module
Slide
Flick
Trace
Multi-touch ownership
```

v0.4.0：

```text
Phigros Module
Dynamic JudgeLine
Arbitrary coordinate transform
```

如果在上述游戏接入过程中：

```text
games/*
```

可以扩展，而：

```text
kernel/*
```

基本无需因玩法差异修改，则说明 HRK 的抽象边界设计成功。

---

# 41. v0.1.0 核心成功标准

可以将整个 v0.1.0 最终压缩为一句工程验收标准：

> 在完全不修改 ReferenceGame 源码的前提下，同一份 RuntimeChart 能分别通过 HeadlessBackend 和 HarmonyBackend 运行；相同 Replay 在 Headless 环境重复执行得到完全一致的判定与得分，同时 HarmonyOS 真机端使用 OHAudio、Native Touch 和 XComponent/OpenGL ES 完成相同 GameplaySession 的实时运行。

只要这一点成立，HRK 就已经具备继续承载 PJSK、Arcaea、Phigros 和 Mania 等具体游戏架构的基础。