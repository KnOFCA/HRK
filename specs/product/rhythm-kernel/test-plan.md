# HRK v0.1.0 测试计划

输入、详细步骤、预期及优先级统一引用 [验收契约](acceptance.md)，不在此重新定义。

| 测试 ID | AC | 原用例组 | 执行方法 | 当前状态 |
|---|---|---|---|---|
| PROD-HRK-TEST-001 | PROD-HRK-AC-001 | TC-BUILD、TC-ARCH、TC-HEAD、TC-BACKEND | 主机静态检查 / 单元与集成测试 | PASS |
| PROD-HRK-TEST-002 | PROD-HRK-AC-002 | TC-BASE、TC-TIME、TC-TL | 主机静态检查 / 单元与集成测试 | PASS |
| PROD-HRK-TEST-003 | PROD-HRK-AC-003 | TC-AUDIO、TC-IN、TC-RT | 主机静态检查 / 单元与集成测试 | PASS |
| PROD-HRK-TEST-004 | PROD-HRK-AC-004 | TC-GAME、TC-CHART、TC-JUDGE、TC-SCORE、TC-AUTO | 主机静态检查 / 单元与集成测试 | PASS |
| PROD-HRK-TEST-005 | PROD-HRK-AC-005 | TC-REP、TC-DET | 主机静态检查 / 单元与集成测试 | PASS |
| PROD-HRK-TEST-006 | PROD-HRK-AC-006 | TC-REN、TC-HAR、TC-E2E、TC-LIFE | 设备实测 + 主机集成与故障注入 | PASS |
| PROD-HRK-TEST-007 | PROD-HRK-AC-007 | TC-RES、TC-DIAG、TC-STAB、TC-MEM、TC-PERF、TC-ERR | 设备实测 + 主机集成与故障注入 | PASS |

每个组覆盖附件中该前缀的全部用例，不用组数代替用例数。执行顺序为构建、静态架构检查、单元、集成、确定性、Harmony、端到端和发布核对。
实际测试入口为 tests/hrk_tests.cpp（逐 TC 断言）、tools/check-architecture.mjs（源码与 target 边界）、tests/DEVICE.md（设备操作）。命令见系统 TEST_PLAN。主机已执行项及尚未执行项以 validation 和证据矩阵为准。
设备不足记 BLOCKED，未执行记 NotRun。每次结果保存工具链、设备、规格版本、源码版本、输入哈希、日志及逐条结论，按 [证据约定](../../../evidence/README.md) 管理。

2026-09-20 完成全部原 TC 的逐项归档：124 PASS，1 UNSUPPORTED_DEVICE（条件性 120 Hz），详见 [验证记录](validation.md)。

## 2026-09-20 真机补测

按用户新授权，安装同一已归档 HAP，在 MRO-W00 补测最终版本的 60 秒性能、120 Hz 请求与判定一致性、平台注入 PERFECT/MISS/多指 Replay、暂停/恢复/seek、后台及 Surface 生命周期。排除需要真人触摸、听感、人工旋转的复测，不以注入事件替代人工证据。保留原模拟器归档，新建真机证据；全部设备操作与导出结束后移除该真机 HDC TCP 连接，并验证已断开。

本次补测已执行并归档；真机调试连接已断开。结果见 [功能验证记录](validation.md)。

## 最终版本真人输入补测（已完成）

按用户后续授权补齐真人证据：手指命中与漏击、同时双指 DOWN/MOVE/UP、保存 Replay 并逐项对比 Headless；随后确认播放/暂停/恢复听感及旋转后的画面与触摸。由用户实际操作和确认，执行 Agent 不注入事件冒充真人。已准备 MRO-W00 Play 页面，结果待采集；不以准备成功标记测试通过。完成后沿用用户偏好断开真机连接。

最终执行：用户完成两轮操作并确认；已导出原始 Replay、核对双指生命周期和完整 Headless 结果，诊断限制已保留；真机连接已断开。详见 [验证记录](validation.md)。

## Linux sanitizer CI 失败调查

首次远程运行配置和构建成功，但 CTest 返回8，SDD步骤未执行。保留ASan/UBSan与失败门槛，先将测试错误摘要发布为工作流annotation以便审查，再按实际诊断修复；Windows和Harmony既有证据不替代此Linux结果。
