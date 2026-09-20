---
id: PROD-HRK
legacy_ids: []
title: Harmony Rhythm Kernel v0.1.0
domain: product
spec_status: Approved
approval: Approved
approver: 当前会话用户
approval_date: 2026-09-19
approval_evidence: 当前会话明确指令“完成v0.1.0迭代”
implementation_status: Complete
validation_status: PASS
release_status: Accepted
canonical_spec: specs/product/rhythm-kernel/spec.md
requirements_version: 3
---

# HRK v0.1.0 功能规格

对应 [系统规格](../../../SPEC.md)。原始需求的规范条款迁移如下，保留原章节编号以便追溯。
本文件及其规范附件 [验收契约](acceptance.md) 为本功能唯一需求来源。原文中的示例、建议仍为示例、建议，不因迁移变成确定契约。
设计性章节转到 design.md，开发顺序转到 tasks.md，未来路线单列 roadmap；旧实现约定仅归档。

## 迁移后的需求索引

| 需求 ID | 范围 | 规范章节 |
|---|---|---|
| PROD-HRK-REQ-001 | 架构与依赖边界 | 原章节 2、5、12—15、22、25—28、34 |
| PROD-HRK-REQ-002 | 时钟与音乐时间轴 | 原章节 6—7 |
| PROD-HRK-REQ-003 | 音频与原始输入 | 原章节 8—9、21 |
| PROD-HRK-REQ-004 | ReferenceGame 与运行时 | 原章节 11、16—18、20 |
| PROD-HRK-REQ-005 | Replay 与确定性 | 原章节 19、31—32、36 |
| PROD-HRK-REQ-006 | Harmony 音频、输入与渲染闭环 | 原章节 10、14、33 |
| PROD-HRK-REQ-007 | 资源、诊断、异常及发布 | 原章节 35、38、41；验收契约 |

| 验收 ID | 需求 | 通过条件 |
|---|---|---|
| PROD-HRK-AC-001 | PROD-HRK-REQ-001 | 检查依赖图和两种 Backend 组合；详细预期见验收契约的 TC-ARCH、TC-HEAD、TC-BACKEND 用例 |
| PROD-HRK-AC-002 | PROD-HRK-REQ-002 | 单调时间、暂停、seek、BPM 双向映射；详细预期见验收契约的 TC-BASE、TC-TIME、TC-TL 用例 |
| PROD-HRK-AC-003 | PROD-HRK-REQ-003 | 音频驱动和原始 timestamp 映射；详细预期见验收契约的 TC-AUDIO、TC-IN、TC-RT 用例 |
| PROD-HRK-AC-004 | PROD-HRK-REQ-004 | 不可变谱面、判定、得分及三种运行模式；详细预期见验收契约的 TC-GAME、TC-CHART、TC-JUDGE、TC-SCORE、TC-AUTO 用例 |
| PROD-HRK-AC-005 | PROD-HRK-REQ-005 | 保存/读取闭环、重复运行及不同帧率一致；详细预期见验收契约的 TC-REP、TC-DET 用例 |
| PROD-HRK-AC-006 | PROD-HRK-REQ-006 | 真实平台 API、触摸、Surface 和生命周期；详细预期见验收契约的 TC-HAR、TC-E2E、TC-LIFE 用例 |
| PROD-HRK-AC-007 | PROD-HRK-REQ-007 | 所有 P0 和 Release Gate 实际通过；详细预期见验收契约的 TC-RES、TC-DIAG、TC-STAB、TC-MEM、TC-PERF、TC-ERR 用例 |

优先级沿用验收契约的 P0 / P1 清单。全部原 TC 编号保留在附件，不伪造已执行结果。

## 工作流概念

| Workflow Step | 行为 | 类型 / Validation Gate |
|---|---|---|
| S1 | 复核迁移后的产品规格与未决项 | 规格审查；审批通过后才能进入实现 |
| S2 | 按任务建立内核、Headless 与 Harmony | Action Step；各 AC 对应测试通过 |
| S3 | 执行全量测试及设备验证 | Validation Gate；验收契约 Release Gate 全部通过 |
| S4 | 保存版本、哈希和验证结论 | Recording Step；证据完整后才能申请发布 |

## 实现前审查（2026-09-19）

执行 Agent 已复核正常、边界、异常、依赖和可测试性；以下补充明确原文可选项，不扩大版本范围。原验收契约的全部 TC 和 Release Gate 保持有效。授权来自当前用户，不将执行 Agent 记为产品批准人。

### 序列化与边界（REQ-004、REQ-005）

- Chart 使用 UTF-8 JSON，根对象包含 bpm（有限数值，0 < bpm <= 1000）和 notes 数组；可选 tempos 数组包含 beat、bpm，首点 beat=0；未提供时使用根 bpm。note 包含 beat（0..1000000）及 x（0..1）。最多 100000 notes、4096 tempos、16 MiB 文本；未知字段、重复字段、非法 JSON、非有限值、重复 tempo beat 拒绝为 ChartParseError。按目标时间及原始次序稳定排序，ID 从 1 开始；编译后只读。
- 时间为 int64 纳秒，歌曲及输入范围为 0..24 小时；超出范围返回 InvalidArgument。Beat-Time 结果就近取整到纳秒，反向误差 <= 1e-7 beat。
- RuleVersion=1；仅 DOWN 判定，时间误差绝对值 <=50 ms 且 x 距离 <=0.10 为 PERFECT（float32 表示边界使用 0.100000024 容差）（含边界）。同时符合的对象选时间最早、再按 ID；一次 DOWN 最多命中一个。PERFECT=100 分、MISS=0。MISS 在 noteTime+50 ms 之后产生，其 inputTime 固定为 noteTime+50 ms+1 ns，避免依赖更新帧。判定按输入和超时的时间顺序分发，超时严格早于输入时先分发超时。
- Replay 为小端二进制：ASCII `HRKR`、u32 formatVersion=1、u32 kernelVersion=1、u32 gameId=1、u64 chartHash、u32 ruleVersion=1、u32 eventCount，然后逐事件 i64 songTime、u32 pointerId、u32 phase（0 DOWN/1 MOVE/2 UP/3 CANCEL）、IEEE754 float32 x/y。事件按 songTime 非递减，同时间保持记录顺序。最多 1000000 事件，文件精确长度必须匹配，不允许尾随字节；坐标必须有限且在 0..1。未知版本返回 UnsupportedVersion；game/rule 不符返回 ReplayIncompatible；谱面不符返回 ReplayChartMismatch；损坏返回 ReplayInvalid。
- chartHash 是对原始 Chart UTF-8 字节的 FNV-1a 64-bit（offset=14695981039346656037、prime=1099511628211）；仅作版本身份校验，非安全认证。重放必须使用相同文件字节。

### 运行时契约（REQ-002、REQ-003、REQ-004）

- Ready 才能 start；Playing 才能 pause；Paused 才能 resume；Ready/Playing/Paused 可以 seek，其他非法转换返回 InvalidState。stop 幂等，停止音频和输入；重新运行需 load 回到 Ready。加载失败不能进入 Playing，平台失败返回 BackendFailure。
- 音频位置驱动 SongClock，单个播放区间单调；暂停冻结，resume 重新建立 host/song anchor。输入使用平台事件原始单调纳秒时间，不能使用 handler 时间。平台可提供同次设备查询得到的 host/audio 时间对，映射允许在当前播放 epoch 内向前或向后换算；开始/恢复/seek 之前的 epoch 不接收输入。映射区间之前的输入返回 StaleInput；乱序、早于上次处理水位的输入拒绝为 LateInput 并计数。每次 update 先处理已接收输入再推进超时。2026-09-19 真机发现事件分发通常晚于上一绘制帧，故 Harmony Play 的超时提交水位保留 40 ms 分发余量，Headless / Replay / AutoPlay 水位无额外延迟；输入命中仍即时按原始时间戳执行。真正超过已提交水位的乱序输入仍拒绝。音频 EOF 后 Play 等待设备音频时间越过该余量再结束，最后统一推进到歌曲终点，不能遗漏末尾输入。平台生产者必须在推进 watermark 前交付已知输入；超出水位的迟到输入不回滚既有结果。
- 输入固定容量 2048、单生产者单消费者；满时拒绝最新事件并增加 overflow 计数，绝不覆盖旧事件。暂停、seek、stop 清空队列和 active pointers；pointerId 保持原值，UP/CANCEL 移除对应状态，最多 32 个并发 pointer。
- seek 重置判定和分数：Replay/AutoPlay 从起点按时间重建到目标（含目标时间输入），Play 按已保留的先前输入重建，已过期且未命中的对象记 MISS，录制按新时间分支截断。seek 在非实时控制路径执行；音频与输入 anchor 同步重建。导出的 Play Replay 必须可重建当前分数。
- Play 录制映射后的事件；Replay/AutoPlay 忽略平台实时输入。自动输入由 Game 生成，Kernel 不包含具体玩法。录制达到 1000000 事件后返回 ReplayFull 并安全暂停，禁止继续产生不可重放结果。
- PCM 最低格式为 WAV PCM16 little-endian、1/2 声道、48000 Hz，拒绝损坏或不支持格式；加载和解码在 start 之前完成，回调只复制 PCM、填零、更新原子位置，不加载、解析、分配或锁等待。

### 测量口径（REQ-007）

- 基线工具先加载与解码资源，等待 3 秒让加载期间积压的平台事件完成，再同时开始歌曲播放与帧采样；准备期间不播放音频，不计入播放测量。采样开始后不剔除慢帧或暂停区间。
- 设备记录型号、系统、刷新率和构建模式；60 FPS 连续采样 60 秒，帧间隔 p95 <=20 ms，且不得连续 1 秒低于 50 FPS。120 Hz 仅在设备支持时请求并测量，不支持记录原因。
- 主机 1000 notes 完整重放小于 1 秒；记录主机与编译器。内存测试预热后重复创建/销毁及 replay 加载各 100 次，使用可用 sanitizer 或系统内存采样，末段常驻内存较基线增长 <=8 MiB；不能用无 crash 代替泄漏验证。
- 没有设备时相关 TC 必须 BLOCKED，不能用源码扫描或 mock 替代真音频、触摸、60 秒性能及跨 Backend Replay 验收。

## 验收范围记录（2026-09-20）

用户明确指示真机移除后使用 DevEco Studio 模拟器继续迭代。最终设备性能基线以该模拟器为目标；既有真机声音、手指输入及旋转证据单独保留。120 Hz 按原条件用例记录 UNSUPPORTED_DEVICE；不推断未测硬件能力。版本验收 Accepted，不代表已对外发布。结果和限制见 [验证记录](validation.md)。

## 原规范章节

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


# 41. v0.1.0 核心成功标准

可以将整个 v0.1.0 最终压缩为一句工程验收标准：

> 在完全不修改 ReferenceGame 源码的前提下，同一份 RuntimeChart 能分别通过 HeadlessBackend 和 HarmonyBackend 运行；相同 Replay 在 Headless 环境重复执行得到完全一致的判定与得分，同时 HarmonyOS 真机端使用 OHAudio、Native Touch 和 XComponent/OpenGL ES 完成相同 GameplaySession 的实时运行。

只要这一点成立，HRK 就已经具备继续承载 PJSK、Arcaea、Phigros 和 Mania 等具体游戏架构的基础。
