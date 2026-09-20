# HRK v0.1.0 验证记录

2026-09-20：implementation_status=Complete，validation_status=PASS，release_status=Accepted（未对外发布）。

本轮从重建代码实际执行主机与设备验证，不沿用旧实现 PASS。七项 AC 全部通过，125 个原 TC 中 124 PASS、1 UNSUPPORTED_DEVICE（原规格允许的条件性 120 Hz）。

实际命令、环境、早期失败、已知限制及发布产物见 [系统验证记录](../../../VALIDATION.md)。逐项映射见 [证据矩阵](../../../evidence/PROD-HRK/2026-09-20-final/acceptance-results.json)，版本与产物哈希见 [manifest](../../../evidence/PROD-HRK/2026-09-20-final/manifest.json)。

真机声音、实际手指及旋转由用户确认；其移除后按用户指示改用 DevEco 模拟器完成最终测试。独立 60 秒基线最低 57 FPS、p95 16.6744 ms。最终模拟器结果不冒充真机性能结果。

## 同日真机补测

用户授权的非人工真机补测完成：21 个关联 TC PASS，120 Hz 条件项 UNSUPPORTED_DEVICE。最终 HAP 真机 60 秒性能最低 55 FPS、p95 16.6707 ms；自动输入、跨 Backend Replay 和生命周期通过。人工输入/听感/旋转未重测。测试完成已移除 HDC 连接并确认列表为空。详见 [补测 manifest](../../../evidence/PROD-HRK/2026-09-20-physical-final/manifest.json) 与系统验证记录，原模拟器证据保持不变。

## 真人补测完成

用户随后实际完成手指命中/双指拖动释放、暂停恢复听感、旋转及旋转后触摸，13 个关联 TC PASS。两轮 Replay 完整结果等于 Headless；诊断 LateInput 17/3、overflow=0 如实保留。测试后已断开真机连接。详见 [真人证据](../../../evidence/PROD-HRK/2026-09-20-human-final/manifest.json) 和系统验证记录。
