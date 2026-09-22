# HRK 测试策略

## v0.1.1 规划

输入可靠性与 LI-001～LI-004 的回归范围见 [测试计划](specs/product/input-reliability/test-plan.md)，通过条件见 [规格](specs/product/input-reliability/spec.md)。TASK-001～003 已完成文档审查、采集、离线重放/最小化工具和对应测试；AC-001 PASS，其余验收尚未完成，真人根因调查留给 TASK-004。

## 文档迁移验收

1. 原始需求、旧文档及已有证据保留快照并校验 SHA-256；迁移不丢失原 TC 编号。
2. 系统文档、功能五件套、三领域入口、模板治理工具齐全，相对链接全部有效。
3. 旧 app、core、tools 中的 HRK 脚本、CI、CMake、格式配置、out 与 .hvigor 已移除；src / tests 只有说明。
4. 当前产品状态明确为 NotStarted / NotRun / NotAccepted，历史证据单独标记。
5. 在 HRK 根目录执行 `bash tools/sdd/check.sh`，覆盖 lint、链接、provenance、工具自测、必需文件及模板演练。

## 产品验收

按 [功能测试计划](specs/product/rhythm-kernel/test-plan.md) 映射全部 AC 和原 TC 用例；门禁由 [功能规格](specs/product/rhythm-kernel/spec.md) 及附件定义。
产品测试从当前源码重新执行；文档检查 PASS 不表示功能或发布通过。

```powershell
cmake --build build/host --config Debug
ctest --test-dir build/host -C Debug --output-on-failure
node tools/check-architecture.mjs
python tools/harmony/harmony_build.py --project src/app/harmony --deveco "<DevEco Studio>" --no-repair-links
```

正式结果逐 TC 保存到 evidence/PROD-HRK；设备步骤见 [DEVICE](tests/DEVICE.md)。
