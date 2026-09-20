# 源码

- kernel：公共类型、时间、时间轴、输入、PCM 解码、资源、Replay 与 GameplaySession。
- interface：Game / Platform / Render 的抽象接口。
- games/reference：只读谱面编译、判定、得分、自动输入及画面命令。
- platform/headless：人工音频时钟、输入、空渲染与帧调度。
- platform/harmony：OHAudio、XComponent 原生触摸、EGL/GLES 与 NAPI 适配。
- app：具体游戏与平台的组合，以及 Harmony ArkTS 外壳。

需求来源：[功能规格](../specs/product/rhythm-kernel/spec.md)。
