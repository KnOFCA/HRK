# HRK v0.1.0 验收契约（规范附件）

本文件属于 [功能规格](spec.md) 的规范附件，完整保留原测试与验收说明中的输入、预期、TC 编号及 Release Gate。
下文是未来验收要求和报告模板，所有示例 PASS 均不代表当前执行结果；当前结果见 [validation](validation.md)。
执行计划仅引用本附件，避免重复定义预期。原文中的代码片段、目录和命令描述未来实现，并不表示仓库已有这些文件。

---

# Harmony Rhythm Kernel v0.1.0 测试与验收说明

版本：v0.1.0  
项目：Harmony Rhythm Kernel  
文档：VALIDATION_v0.1.0  
对应规格：SPEC_v0.1.0

---

# 1. 测试目标

HRK v0.1.0 的测试重点不是验证复杂音游玩法，而是验证以下核心能力：

1. Kernel、Game、Platform 三层依赖边界正确；
2. 时间系统具备确定性；
3. Beat-Time 映射正确；
4. 输入事件使用原始时间戳判定；
5. Headless Backend 能独立运行完整 GameplaySession；
6. ReferenceGame 能完成最小 Chart → Input → Judgment → Score 闭环；
7. Replay 能复现完全一致的结果；
8. HarmonyOS Backend 能完成 Audio / Touch / Render 的真实平台闭环；
9. Gameplay 结果不依赖帧率；
10. 同一个 Game Module 无需修改即可运行于 Headless 与 Harmony Backend。

测试分为：

```text
Unit Test
    ↓
Kernel Integration Test
    ↓
Game Integration Test
    ↓
Headless Determinism Test
    ↓
Harmony Platform Test
    ↓
End-to-End Acceptance Test
```

---

# 2. 测试环境

## 2.1 Host 测试环境

推荐：

```text
Windows / Linux / WSL
CMake
Ninja
C++17 compiler
```

用于执行：

```text
hrk_tests
```

Headless 测试不得依赖：

```text
HarmonyOS SDK
GPU
Audio Device
Touch Device
```

---

## 2.2 HarmonyOS 测试环境

至少需要：

```text
DevEco Studio
HarmonyOS Native SDK
CMake / Ninja
支持 XComponent 的设备或模拟环境
```

正式低延迟音频与真实 Touch Timestamp 验证推荐真机。

---

# 3. 测试数据

建立固定测试数据目录：

```text
tests/
├─ data/
│  ├─ charts/
│  │  ├─ single_note.json
│  │  ├─ simple_sequence.json
│  │  ├─ bpm_change.json
│  │  └─ empty_chart.json
│  │
│  ├─ replay/
│  │  ├─ perfect.replay
│  │  ├─ miss.replay
│  │  └─ mixed.replay
│  │
│  └─ audio/
│     ├─ click_120bpm.wav
│     └─ short_test.wav
│
└─ expected/
   ├─ simple_sequence_result.json
   └─ mixed_result.json
```

所有自动化测试必须使用固定输入。

---

# 4. ReferenceGame 统一测试规则

ReferenceGame 在 v0.1.0 中使用固定判定规则。

例如：

```text
PERFECT:
|input_time - note_time| <= 50 ms

MISS:
超过判定窗口仍未命中
```

位置判定：

```text
|input.x - note.x| <= 0.10
```

所有自动测试必须使用同一 Rule Version。

例如：

```text
ReferenceRuleVersion = 1
```

---

# 5. Base 模块测试

## TC-BASE-001 TimeNs 基础运算

### 目的

验证纳秒时间类型不会因普通算术产生错误。

### 输入

```text
t1 = 1,000,000,000 ns
t2 =   500,000,000 ns
```

### 操作

```text
t1 + t2
t1 - t2
```

### 预期

```text
1,500,000,000 ns
500,000,000 ns
```

### 通过条件

结果完全一致。

---

## TC-BASE-002 时间单位转换

### 输入

```text
1,500,000,000 ns
```

### 预期

```text
seconds = 1.5
milliseconds = 1500
```

---

## TC-BASE-003 EntityId 唯一性

创建：

```text
10000 EntityId
```

### 预期

无重复 ID。

---

# 6. Clock 测试

## TC-TIME-001 SongClock 初始状态

创建：

```text
SongClock
```

### 预期

```text
state = Stopped
time = 0
```

---

## TC-TIME-002 Start

### 操作

```text
clock.start()
advance audio 100 ms
```

### 预期

```text
SongTime ≈ 100 ms
state = Playing
```

Headless 中应精确等于模拟时间。

---

## TC-TIME-003 Pause

### 操作

```text
start
advance 1000 ms
pause
advance host 500 ms
```

### 预期

```text
SongTime = 1000 ms
```

暂停期间歌曲时间不得继续推进。

---

## TC-TIME-004 Resume

### 操作

```text
start
advance 1000 ms
pause
advance 500 ms
resume
advance 200 ms
```

### 预期

```text
SongTime = 1200 ms
```

而不是：

```text
1700 ms
```

---

## TC-TIME-005 Seek

### 操作

```text
seek(5000 ms)
```

### 预期

```text
SongTime = 5000 ms
```

之后继续：

```text
advance 100 ms
```

预期：

```text
5100 ms
```

---

## TC-TIME-006 Monotonic

连续读取：

```text
t0
t1
t2
...
```

### 预期

Playing 状态：

```text
t[n+1] >= t[n]
```

不允许时间倒退。

Seek 除外。

---

# 7. ClockMapper 测试

## TC-TIME-007 Host → Song 映射

建立映射：

```text
Host = 10.000 s
Song = 4.000 s
```

输入事件发生：

```text
Host = 10.025 s
```

### 预期

```text
Song = 4.025 s
```

---

## TC-TIME-008 输入事件跨 Render Frame

假设：

```text
Frame A = 1000 ms
Touch   = 1005 ms
Frame B = 1016 ms
```

### 预期

Touch SongTime：

```text
1005 ms
```

不得使用：

```text
1016 ms
```

---

# 8. Timeline 测试

## TC-TL-001 单 BPM Beat → Time

谱面：

```text
BPM = 120
```

预期：

```text
Beat 0 = 0 ms
Beat 1 = 500 ms
Beat 2 = 1000 ms
Beat 4 = 2000 ms
```

---

## TC-TL-002 Time → Beat

BPM：

```text
120
```

输入：

```text
0 ms
250 ms
500 ms
1250 ms
```

预期：

```text
0
0.5
1
2.5
```

---

## TC-TL-003 BPM Change

配置：

```text
Beat 0: BPM 120
Beat 4: BPM 240
```

预期：

```text
Beat 4 = 2000 ms
Beat 5 = 2250 ms
Beat 6 = 2500 ms
```

---

## TC-TL-004 双向映射一致性

随机选取多个 Beat：

```text
0
0.25
1
3.5
4
5.25
16
```

执行：

```text
beat
→ time
→ beat'
```

### 预期

```text
|beat - beat'| < epsilon
```

---

## TC-TL-005 空 Timeline

### 输入

无 TempoPoint。

### 预期

必须：

```text
返回明确错误
```

不得：

```text
crash
division by zero
undefined behavior
```

---

# 9. Chart Loader 测试

## TC-CHART-001 正常加载

输入：

```json
{
  "bpm": 120,
  "notes": [
    {"beat": 1, "x": 0.25},
    {"beat": 2, "x": 0.50}
  ]
}
```

### 预期

RuntimeChart：

```text
note count = 2
```

并得到：

```text
500 ms
1000 ms
```

两个目标时间。

---

## TC-CHART-002 非法 BPM

输入：

```text
BPM = 0
```

### 预期

加载失败并返回明确错误。

---

## TC-CHART-003 非法 Note Position

输入：

```text
x = 1.5
```

若 ReferenceGame 定义：

```text
0 <= x <= 1
```

则必须报告非法谱面。

---

## TC-CHART-004 RuntimeChart 不可变

Chart compile 完成后：

```text
GameplaySession.start()
```

### 验收

RuntimeChart API 不提供运行时：

```text
addNote()
removeNote()
setNoteTime()
```

等 mutation 方法。

---

# 10. Input 模块测试

## TC-IN-001 DOWN 事件

输入：

```text
pointer = 1
phase = DOWN
x = 0.5
hostTime = 1000 ms
```

### 预期

输出：

```text
pointer = 1
phase = DOWN
x = 0.5
songTime = mapped value
```

---

## TC-IN-002 MOVE 顺序

输入：

```text
DOWN @1000
MOVE @1010
MOVE @1020
UP   @1030
```

### 预期

事件输出顺序完全一致。

---

## TC-IN-003 Pointer ID 保持

输入两个 pointer：

```text
Pointer 3
Pointer 7
```

### 预期

Kernel 不得重新映射成无法追踪原始输入的 ID。

---

## TC-IN-004 多点输入

输入：

```text
Pointer 1 DOWN
Pointer 2 DOWN
Pointer 1 MOVE
Pointer 2 UP
Pointer 1 UP
```

### 预期

两个 pointer 状态独立。

---

## TC-IN-005 CANCEL

输入：

```text
DOWN
MOVE
CANCEL
```

### 预期

pointer 生命周期结束。

不得留下 active pointer。

---

# 11. Input Queue 测试

## TC-IN-006 FIFO

顺序写入：

```text
A B C D
```

读取：

```text
A B C D
```

---

## TC-IN-007 Queue Burst

连续写入：

```text
1000 input events
```

### 验收

不得：

```text
crash
memory corruption
wrong ordering
```

若队列容量不足，必须有明确 overflow policy。

例如：

```text
fail test
drop newest
```

不得静默产生不可预测行为。

---

# 12. ReferenceGame Judgment 测试

假设：

```text
Note time = 1000 ms
Note x = 0.5
```

## TC-JUDGE-001 Perfect Zero Error

输入：

```text
Touch = 1000 ms
x = 0.5
```

预期：

```text
PERFECT
error = 0
```

---

## TC-JUDGE-002 Early Perfect Boundary

输入：

```text
950 ms
```

预期：

```text
PERFECT
error = -50 ms
```

---

## TC-JUDGE-003 Late Perfect Boundary

输入：

```text
1050 ms
```

预期：

```text
PERFECT
error = +50 ms
```

---

## TC-JUDGE-004 Early Outside Window

输入：

```text
949 ms
```

预期：

```text
不得命中
```

最终 Note：

```text
MISS
```

---

## TC-JUDGE-005 Late Outside Window

输入：

```text
1051 ms
```

预期：

```text
MISS
```

---

## TC-JUDGE-006 Position Miss

输入：

```text
time = 1000 ms
x = 0.8
```

而 Note：

```text
x = 0.5
```

### 预期

```text
MISS
```

---

## TC-JUDGE-007 Note 只能判定一次

输入：

```text
Touch @ 1000 ms
Touch @ 1010 ms
```

### 预期

只产生一个：

```text
PERFECT
```

第二次输入不得再次命中同一 Note。

---

# 13. Miss Scheduler 测试

## TC-JUDGE-008 无输入自动 Miss

Note：

```text
1000 ms
```

时间推进到：

```text
1100 ms
```

无 Touch。

### 预期

产生一次：

```text
MISS
```

---

## TC-JUDGE-009 Miss 不重复产生

时间继续：

```text
2000 ms
3000 ms
```

### 预期

同一 Note 始终只有一个 MISS。

---

# 14. Score 测试

假设：

```text
PERFECT = +100
MISS = 0
```

## TC-SCORE-001 Single Perfect

输入：

```text
1 PERFECT
```

预期：

```text
score = 100
perfect = 1
miss = 0
```

---

## TC-SCORE-002 Mixed

输入：

```text
P
P
M
P
```

预期：

```text
score = 300
perfect = 3
miss = 1
```

---

## TC-SCORE-003 Reset

先产生：

```text
score = 500
```

调用：

```text
reset()
```

预期：

```text
score = 0
perfect = 0
miss = 0
```

---

# 15. GameplaySession 生命周期测试

## TC-GAME-001 正常生命周期

执行：

```text
Created
→ Loading
→ Ready
→ Playing
→ Finished
```

### 预期

每个状态转换有效。

---

## TC-GAME-002 Pause / Resume

执行：

```text
Playing
→ Paused
→ Playing
```

### 验收

暂停期间：

```text
不产生新 Miss
SongClock 不推进
```

恢复后正常继续。

---

## TC-GAME-003 Stop

Playing 状态调用：

```text
stop()
```

### 预期

```text
state = Finished / Stopped
```

Audio 停止。

Input 不再进入 Gameplay。

---

## TC-GAME-004 非法状态转换

例如：

```text
Created → Resume
```

### 预期

返回错误。

不得 crash。

---

# 16. Audio Decoder 测试

## TC-AUDIO-001 WAV Decode

输入：

```text
short_test.wav
```

### 验收

得到：

```text
sampleRate > 0
channels > 0
frameCount > 0
```

---

## TC-AUDIO-002 EOF

读取整个文件。

### 预期

EOF 正常返回。

不得越界读取。

---

## TC-AUDIO-003 Seek

解码：

```text
seek 1000 ms
```

### 验收

下一 PCM frame 对应目标附近位置。

---

# 17. FakeAudioBackend 测试

## TC-AUDIO-004 Manual Advance

执行：

```text
start
advance(500 ms)
```

预期：

```text
audio position = 500 ms
```

---

## TC-AUDIO-005 Pause

```text
advance 500
pause
advance 500
```

预期：

```text
position = 500 ms
```

---

## TC-AUDIO-006 Resume

之后：

```text
resume
advance 250
```

预期：

```text
position = 750 ms
```

---

# 18. AudioClock 集成测试

## TC-AUDIO-007 Audio Drives SongClock

FakeAudio：

```text
position = 1234 ms
```

### 预期

SongClock：

```text
≈ 1234 ms
```

不应由 Frame delta 单独推进。

---

# 19. Replay Record 测试

## TC-REP-001 Record

输入：

```text
DOWN @950
UP @970
DOWN @1500
UP @1520
```

### 预期

Replay 保存：

```text
4 events
顺序一致
timestamp 一致
pointer 一致
position 一致
```

---

## TC-REP-002 Header

Replay 必须包含：

```text
formatVersion
gameId
chartHash
ruleVersion
```

---

# 20. Replay Load 测试

## TC-REP-003 Save → Load

执行：

```text
Replay A
→ Serialize
→ Deserialize
→ Replay B
```

### 预期

```text
A == B
```

---

## TC-REP-004 Wrong Version

输入未知：

```text
formatVersion
```

### 预期

明确拒绝加载。

不得尝试按当前格式强制解析。

---

## TC-REP-005 Wrong Chart Hash

Replay：

```text
chartHash = A
```

当前谱面：

```text
chartHash = B
```

### 预期

返回：

```text
ReplayChartMismatch
```

---

# 21. Replay Determinism 核心测试

这是 v0.1.0 最重要测试之一。

## TC-DET-001 单次重放

Chart：

```text
simple_sequence
```

Replay：

```text
mixed.replay
```

预期：

```text
P
P
M
P
```

且 Score 与 expected 完全一致。

---

## TC-DET-002 重复 100 次

同一个：

```text
Chart
Replay
RuleVersion
```

执行：

```text
100 runs
```

### 预期

每次：

```text
Judgment count
Judgment order
Timing error
Score
```

完全一致。

不得只比较最终 Score。

---

## TC-DET-003 不同 Render FPS

模拟：

```text
30 Hz
60 Hz
90 Hz
120 Hz
144 Hz
```

运行相同 Replay。

### 预期

Gameplay Result 完全相同。

---

## TC-DET-004 不规则 Render FPS

模拟 frame：

```text
16 ms
16 ms
33 ms
8 ms
20 ms
12 ms
...
```

### 预期

结果与固定 60 FPS 完全一致。

---

## TC-DET-005 无 Render

完全不调用：

```text
render()
```

只运行：

```text
clock
input
rules
```

### 预期

Gameplay Result 仍完全一致。

这是验证 Judgment 与 Render 解耦的关键测试。

---

# 22. AutoPlay 测试

## TC-AUTO-001 完美自动输入

ReferenceGame 自动为每个 Note 生成：

```text
Touch @ exact note time
```

### 预期

所有：

```text
PERFECT
```

无 MISS。

---

# 23. Render API 单测

## TC-REN-001 Submit Sprite

GameRenderer：

```text
submit one sprite
```

NullRenderBackend 应收到：

```text
1 SpriteDrawCommand
```

---

## TC-REN-002 Game 不接触 GLES

架构扫描必须验证：

```text
src/games/**
```

不存在：

```text
#include <GLES...
#include <EGL...
```

---

## TC-REN-003 Render 不影响 Judgment

关闭：

```text
hrk_render
```

或使用：

```text
NullRenderBackend
```

执行 replay。

### 预期

Judgment 不变。

---

# 24. Headless Backend 集成测试

## TC-HEAD-001 Full Session

Headless 下执行：

```text
Load Chart
Create Game
Create Runtime
Load Replay
Start
Advance FakeAudio
Process Input
Finish
```

### 预期

完整结束，无平台依赖。

---

## TC-HEAD-002 No GPU

CI 环境：

```text
无 OpenGL Context
```

### 预期

所有 Kernel / Game 测试可通过。

---

## TC-HEAD-003 No Audio Device

不提供真实 Audio Device。

### 预期

FakeAudio 正常驱动游戏。

---

# 25. CMake Target 测试

## TC-BUILD-001 Kernel Host Build

必须能够单独构建：

```text
hrk_base
hrk_time
hrk_timeline
hrk_input
hrk_replay
hrk_runtime
```

无需 Harmony SDK。

---

## TC-BUILD-002 Headless Build

构建：

```text
hrk_tests
```

不得链接：

```text
OHAudio
XComponent
EGL
GLES
```

---

## TC-BUILD-003 Harmony Build

Harmony 构建最终生成：

```text
libhrk_entry.so
```

---

## TC-BUILD-004 Game Independent Build

必须能够单独构建：

```text
hrk_game_reference
```

且不链接：

```text
hrk_platform_harmony
```

---

# 26. Architecture Boundary 测试

建议加入 CI source scanner。

## TC-ARCH-001 Kernel 不依赖 Game

扫描：

```text
src/kernel/
```

禁止出现：

```text
games/
pjsk
arcaea
phigros
```

---

## TC-ARCH-002 Kernel 不依赖 Harmony

扫描：

```text
src/kernel/
```

禁止：

```text
ohaudio
native_window
xcomponent
GLES
EGL
napi
```

---

## TC-ARCH-003 Game 不依赖 Platform

扫描：

```text
src/games/
```

禁止：

```text
platform/
ohaudio
GLES
EGL
xcomponent
```

---

## TC-ARCH-004 Platform 不依赖 Game

扫描：

```text
src/platform/
```

禁止：

```text
games/
IGame-specific implementation
```

---

## TC-ARCH-005 Third-party Header 不泄漏

公共 Header：

```text
src/kernel/**/include
src/interface/**
```

禁止：

```text
#include <miniaudio.h>
```

miniaudio 只能出现在 implementation。

---

# 27. Harmony RenderSurface 测试

## TC-HAR-001 XComponent 创建

启动页面。

### 预期

Native XComponent Surface 创建成功。

---

## TC-HAR-002 EGL Context

### 预期

成功创建：

```text
EGL Display
EGL Context
EGL Surface
```

---

## TC-HAR-003 Clear Screen

调用：

```text
clear()
```

### 预期

屏幕显示预期清屏结果。

---

## TC-HAR-004 Resize

改变窗口尺寸或重新创建 Surface。

### 预期

Renderer 正确更新 viewport。

不得 crash。

---

# 28. Harmony Touch 测试

## TC-HAR-IN-001 DOWN

触摸屏幕。

### 日志或 diagnostics 应显示

```text
pointer id
DOWN
x
y
timestamp
```

---

## TC-HAR-IN-002 MOVE

拖动手指。

### 预期

产生连续 MOVE。

---

## TC-HAR-IN-003 UP

松开。

### 预期

正确结束 pointer 生命周期。

---

## TC-HAR-IN-004 Multi-touch

使用两指。

### 预期

两个 pointer ID 独立。

---

## TC-HAR-IN-005 Touch Timestamp

检查：

```text
event timestamp
```

必须来自输入事件，而不是调用 handler 时重新读取当前时间。

---

# 29. Harmony Audio 测试

## TC-HAR-AUD-001 Start Playback

加载：

```text
short_test.wav
```

### 预期

能够正常播放。

---

## TC-HAR-AUD-002 Position

播放过程中查询：

```text
AudioPosition
```

### 预期

持续增加。

---

## TC-HAR-AUD-003 Pause

暂停。

### 预期

声音停止，AudioPosition 不继续正常推进。

---

## TC-HAR-AUD-004 Resume

恢复。

### 预期

继续从暂停位置播放。

---

## TC-HAR-AUD-005 Seek

seek：

```text
50%
```

### 预期

音频跳转到目标附近。

SongClock 同步更新。

---

# 30. Harmony Frame Test

## TC-HAR-FRAME-001 Frame Callback

启动后统计 callback。

### 预期

持续收到 frame signal。

---

## TC-HAR-FRAME-002 Callback 轻量

Frame callback 不允许直接：

```text
parse chart
decode whole file
blocking IO
```

通过代码审查确认。

---

## TC-HAR-FRAME-003 Frame Rate Independent Judgment

设备分别运行：

```text
60 Hz
120 Hz
```

若硬件支持。

使用同一自动 replay。

### 预期

判定结果相同。

---

# 31. Harmony End-to-End 测试

## TC-E2E-001 ReferenceGame 正常游玩

启动：

```text
ReferenceGame
```

执行真实 Touch。

### 预期

完整路径：

```text
Touch
 ↓
HarmonyInputBackend
 ↓
Input Queue
 ↓
ClockMapper
 ↓
Game Input
 ↓
Game Rules
 ↓
Judgment
 ↓
Score
 ↓
HUD
```

均正常。

---

## TC-E2E-002 Perfect

测试 Note 到达目标时间时点击。

### 预期

HUD：

```text
PERFECT +1
```

---

## TC-E2E-003 Miss

不点击一个 Note。

### 预期

HUD：

```text
MISS +1
```

---

## TC-E2E-004 Replay Record

完成一次游玩。

### 预期

生成有效 Replay。

---

## TC-E2E-005 Replay Headless 重放

把真机产生的 Replay 输入 Headless Runtime。

### 预期

Headless 产生相同：

```text
Judgment Sequence
Score
```

这是一个非常关键的跨 Backend 验收。

---

# 32. Backend 等价性测试

## TC-BACKEND-001 Same Game Module

必须使用同一个：

```text
hrk_game_reference
```

分别链接：

```text
headless
```

与：

```text
harmony
```

不得为两个平台创建：

```text
ReferenceGameHarmony
ReferenceGameHeadless
```

这类分叉实现。

---

## TC-BACKEND-002 Same RuntimeChart

同一谱面文件编译出的 RuntimeChart 逻辑结构必须一致。

---

## TC-BACKEND-003 Same Replay

同一 Replay 在两个 Backend 中：

### Headless

结果作为标准值。

### Harmony Replay Mode

预期产生相同结果。

---

# 33. Pause / Resume E2E

## TC-E2E-006 Pause

播放至：

```text
5 s
```

Pause 2 秒。

### 预期

SongTime 仍：

```text
约 5 s
```

不得变成：

```text
7 s
```

---

## TC-E2E-007 Resume

Resume 1 秒后：

### 预期

```text
SongTime ≈ 6 s
```

---

# 34. Lifecycle 测试

## TC-LIFE-001 Background

进入后台。

### 预期

Gameplay 自动进入安全状态：

```text
Paused
```

Audio 停止或暂停。

---

## TC-LIFE-002 Foreground

回到前台。

### 预期

不会自动错误跳过大量歌曲时间。

---

## TC-LIFE-003 Surface Destroy

销毁 XComponent Surface。

### 预期

Renderer 释放对应资源。

Kernel Gameplay State 不应因此损坏。

---

## TC-LIFE-004 Surface Recreate

重新创建 Surface。

### 预期

Renderer 能恢复。

---

# 35. Resource 测试

## TC-RES-001 Load Existing

加载存在的测试资源。

### 预期

成功。

---

## TC-RES-002 Missing Resource

加载不存在文件。

### 预期

返回明确：

```text
ResourceNotFound
```

不得 crash。

---

# 36. Diagnostics 测试

## TC-DIAG-001 Frame Statistics

至少能读取：

```text
frame count
FPS
```

---

## TC-DIAG-002 Input Count

记录：

```text
input event count
```

---

## TC-DIAG-003 Judgment Count

Diagnostics 与 ScoreSystem 中：

```text
Perfect
Miss
```

统计必须一致。

---

# 37. 稳定性测试

## TC-STAB-001 连续 Start/Stop

执行：

```text
start
stop
```

100 次。

### 预期

无 crash。

---

## TC-STAB-002 Pause/Resume Stress

执行：

```text
pause/resume
```

100 次。

### 预期

状态机有效，无死锁。

---

## TC-STAB-003 Seek Stress

随机 seek 100 次。

### 预期

无：

```text
crash
negative invalid state
stuck audio callback
```

---

# 38. 内存安全测试

## TC-MEM-001 GameplaySession 重建

重复：

```text
create
play
destroy
```

100 次。

### 预期

内存占用不持续明显增长。

---

## TC-MEM-002 Replay Load

重复加载 Replay。

### 预期

无泄漏趋势。

---

# 39. 实时路径限制测试

## TC-RT-001 Audio Callback Allocation

通过 instrumentation 或代码审查确认：

Audio callback 中不得进行：

```text
file IO
chart parsing
large heap allocation
sleep
blocking mutex
```

---

## TC-RT-002 Input Hot Path

持续输入大量 MOVE。

### 预期

无明显 heap allocation spike。

---

# 40. 性能基础测试

v0.1.0 不要求完整性能调优，但至少记录基线。

## TC-PERF-001 60 FPS

ReferenceGame 运行 60 秒。

### 预期

在目标设备上无明显持续掉帧。

---

## TC-PERF-002 120 FPS

若设备支持：

### 预期

能够请求并运行高刷新率。

此项允许记录为：

```text
PASS
UNSUPPORTED_DEVICE
```

---

## TC-PERF-003 1000 Notes Headless

生成：

```text
1000 notes
```

完整 Replay。

### 预期

Headless 可完成。

不得出现明显算法级退化。

---

# 41. 错误处理测试

## TC-ERR-001 Invalid Chart

输入损坏 JSON。

### 预期

返回 ChartParseError。

---

## TC-ERR-002 Audio Load Failure

文件不存在。

### 预期

Session 不进入 Playing。

---

## TC-ERR-003 Backend Init Failure

模拟 AudioBackend 初始化失败。

### 预期

返回明确错误。

不得空指针 crash。

---

# 42. Test Matrix

最终测试矩阵：

| 模块 | Unit | Headless Integration | Harmony Integration | E2E |
|---|---:|---:|---:|---:|
| Base | ✓ | | | |
| Time | ✓ | ✓ | ✓ | ✓ |
| Timeline | ✓ | ✓ | | ✓ |
| Audio | ✓ | ✓ | ✓ | ✓ |
| Input | ✓ | ✓ | ✓ | ✓ |
| Gameplay | ✓ | ✓ | ✓ | ✓ |
| Replay | ✓ | ✓ | ✓ | ✓ |
| Render | ✓ | ✓ | ✓ | ✓ |
| Game API | | ✓ | ✓ | ✓ |
| Platform API | | ✓ | ✓ | ✓ |
| Architecture | ✓ | | | |
| Determinism | | ✓ | ✓ | ✓ |

---

# 43. 自动化测试分组

推荐：

```text
ctest
│
├─ hrk_base_tests
├─ hrk_time_tests
├─ hrk_timeline_tests
├─ hrk_input_tests
├─ hrk_audio_tests
├─ hrk_replay_tests
├─ hrk_runtime_tests
├─ hrk_reference_game_tests
├─ hrk_determinism_tests
└─ hrk_architecture_tests
```

其中 CI 每次 commit 必须运行：

```text
base
time
timeline
input
replay
runtime
reference game
determinism
architecture
```

Harmony Device Test 可作为独立 pipeline。

---

# 44. 必须通过的 P0 用例

v0.1.0 Release 前，以下测试全部属于 P0：

```text
TC-TIME-003 Pause
TC-TIME-007 Clock Mapping
TC-TL-003 BPM Change
TC-IN-005 CANCEL

TC-JUDGE-001 Perfect
TC-JUDGE-008 Auto Miss

TC-REP-003 Save/Load
TC-DET-002 100x Determinism
TC-DET-003 FPS Independence
TC-DET-005 No Render Determinism

TC-HEAD-001 Full Headless Session

TC-ARCH-001 Kernel !→ Game
TC-ARCH-002 Kernel !→ Harmony
TC-ARCH-003 Game !→ Platform
TC-ARCH-004 Platform !→ Game

TC-HAR-IN-005 Real Input Timestamp
TC-HAR-AUD-002 Audio Position
TC-E2E-001 Full Gameplay
TC-E2E-005 Harmony Replay → Headless
```

任意 P0 Failure：

```text
v0.1.0 不允许发布
```

---

# 45. P1 用例

P1 包括：

```text
120 FPS
stress seek
resource failure
diagnostics
memory observation
```

原则上 Release 前应通过。

个别设备相关测试允许：

```text
UNSUPPORTED
```

但必须记录原因。

---

# 46. 测试结果报告格式

建议 CI 输出：

```text
HRK v0.1.0 Validation

Build:
  Host .............. PASS
  Headless .......... PASS
  Harmony ........... PASS

Unit:
  Base .............. 8/8
  Time .............. 12/12
  Timeline .......... 9/9
  Input ............. 10/10
  Replay ............ 8/8

Integration:
  Headless .......... PASS
  ReferenceGame ..... PASS

Determinism:
  100 runs .......... IDENTICAL
  30 FPS ............ PASS
  60 FPS ............ PASS
  120 FPS ........... PASS
  No Render ......... PASS

Architecture:
  Kernel → Game ..... NONE
  Kernel → Harmony .. NONE
  Game → Platform ... NONE
  Platform → Game ... NONE

Harmony:
  Audio ............. PASS
  Input ............. PASS
  Render ............ PASS
  Pause/Resume ...... PASS

Overall:
  PASS
```

---

# 47. v0.1.0 Release Gate

只有同时满足以下条件才可标记：

```text
HRK v0.1.0
```

### Build

- Host Build PASS；
- Headless Build PASS；
- Harmony Build PASS。

### Architecture

- Kernel 无 Harmony 依赖；
- Kernel 无 Game 依赖；
- Game 无 Platform 依赖；
- Platform 无 Game 依赖。

### Core

- SongClock 测试全部通过；
- TimingMap 测试全部通过；
- Input Timestamp 测试通过；
- ReferenceGame 完整闭环通过。

### Determinism

同一：

```text
Chart
Replay
RuleVersion
```

连续运行：

```text
100 次
```

结果完全一致。

### Backend

同一：

```text
ReferenceGame
```

无需修改即可运行：

```text
Headless
Harmony
```

### Harmony

能够完成：

```text
Audio
+
SongClock
+
Touch
+
Judgment
+
Score
+
Render
```

实时闭环。

### Replay

Harmony 产生的 Replay 可以在 Headless 环境中复现相同 Gameplay Result。

---

# 48. 最终验收场景

最终建议保留一个固定的 Release Acceptance Scenario。

谱面：

```text
BPM = 120

Note 1:
Beat = 1
x = 0.25

Note 2:
Beat = 2
x = 0.50

Note 3:
Beat = 3
x = 0.75

Note 4:
Beat = 4
x = 0.50
```

对应时间：

```text
500 ms
1000 ms
1500 ms
2000 ms
```

Replay：

```text
Touch 1:
500 ms
x = 0.25

Touch 2:
1020 ms
x = 0.50

Note 3:
No input

Touch 4:
1960 ms
x = 0.50
```

预期：

```text
Note 1 → PERFECT   0 ms

Note 2 → PERFECT  +20 ms

Note 3 → MISS

Note 4 → PERFECT  -40 ms
```

最终：

```text
PERFECT = 3
MISS = 1
```

如果：

```text
Headless
Harmony Replay Mode
30 FPS simulation
60 FPS simulation
120 FPS simulation
No Render simulation
```

全部得到完全相同结果，则 v0.1.0 的最核心目标达成。

---

# 49. v0.1.0 测试完成标准

本版本最终不是通过“画面看起来能玩”来验收，而是通过以下性质验收：

```text
Deterministic
Audio-clock-driven
Timestamped-input
Backend-independent
Game-independent
Replayable
Headless-testable
```

只有当这些性质全部得到自动化测试证明后，才适合在后续版本开始接入：

```text
Mania
Arcaea
PJSK
Phigros
```

等真实玩法。
