# HRK v0.1.0 实现任务

2026-09-19 用户授权完成 v0.1.0；规格歧义已在 [规格](spec.md) 的实现前审查中明确。按以下依赖顺序实施，只有对应全部验收通过才标记 DONE。

| 任务 ID | 需求 | 受影响位置与工作 | 前置任务尾号 | 测试 | 完成条件 | 状态 |
|---|---|---|---|---|---|---|
| PROD-HRK-TASK-001 | PROD-HRK-REQ-001 | `src/` 下 interface、platform、games 的接口与组合；`tests/` | 无 | PROD-HRK-TEST-001 | 对应 AC 全部通过 | DONE |
| PROD-HRK-TASK-002 | PROD-HRK-REQ-002 | `src/` 下 kernel/base、time、timeline；`tests/` | 001 | PROD-HRK-TEST-002 | 对应 AC 全部通过 | DONE |
| PROD-HRK-TASK-003 | PROD-HRK-REQ-003 | `src/` 下 kernel/audio、input 及平台适配；`tests/` | 001、002 | PROD-HRK-TEST-003 | 对应 AC 全部通过 | DONE |
| PROD-HRK-TASK-004 | PROD-HRK-REQ-004 | `src/` 下 games/reference、kernel/gameplay；`tests/` | 002、003 | PROD-HRK-TEST-004 | 对应 AC 全部通过 | DONE |
| PROD-HRK-TASK-005 | PROD-HRK-REQ-005 | `src/` 下 kernel/replay 与 Headless 集成；`tests/` | 004 | PROD-HRK-TEST-005 | 对应 AC 全部通过 | DONE |
| PROD-HRK-TASK-006 | PROD-HRK-REQ-006 | `src/` 下 platform/harmony、kernel/render、app/harmony；`tests/` | 004、005 | PROD-HRK-TEST-006 | 对应 AC 全部通过 | DONE |
| PROD-HRK-TASK-007 | PROD-HRK-REQ-007 | `src/` 下 资源/诊断、集成测试与证据；`tests/` | 006 | PROD-HRK-TEST-007 | 对应 AC 全部通过 | DONE |

## 原推荐开发顺序（计划参考）

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
