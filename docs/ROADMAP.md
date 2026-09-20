# HRK 后续路线（规划参考）

以下沿用原始愿景，不属于当前实现授权或 v0.1.0 验收。

## v0.1.1 本轮规划范围

本轮以 [输入可靠性规格草案](../specs/product/input-reliability/spec.md) 规划 LI-001～LI-004：原始输入轨迹、根因与事件分类、判定/pointer影响、证据驱动修复及真机/Headless回归。当前授权仅限文档，尚未编码和测试。

下方原始愿景中的完整 AudioClock、Calibration 等不自动纳入本次整改；后续另行规格化。

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
