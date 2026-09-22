# 变更记录

## 2026-09-22 CI 治理修复

- 复现规划提交 `06df603` 的 C9 工作流术语缺失；随 TASK-001 定稿规格提交修复，保留严格治理检查。
- Actions checkout 获取完整历史与 tags，确保新归档证据的稳定引用在 CI 中可解析。

## 2026-09-21 v0.1.1 TASK-003 原始输入调查工具

- 新增主机 `hrk_trace`：严格 JSONL 验证、记录时钟/原调度重放、完整结果差异及拒绝事件依赖闭包最小化；不修改既有游戏输入策略。
- 新增 RecordedAudioBackend、五组合成单变量对照和 CLI 失败路径测试。主机 CTest 5/5、CLI 100 项与采集 28 项通过，证据见功能 validation。
- TASK-001～003 完成并提交；真人根因、S4 修复策略及整体验收仍留给后续任务。

## 2026-09-21 v0.1.1 TASK-002 输入诊断采集

- 新增默认关闭的固定容量输入轨迹，关联平台入口/队列/Session/时钟/控制/水位，按 phase、终态和 reason 独立计数；诊断失败不改变游戏行为。
- 新增冻结、超时重试和不覆盖已有文件的 JSONL 导出；Harmony Native 暴露 beginCapture/endCapture/exportCapture。
- task002/AC-001 的主机测试、旧 Replay 回归及 Native 编译通过；真实设备采集、根因调查、原始调度工具和修复仍属后续任务。证据见功能 validation。


## 2026-09-21 v0.1.1 TASK-001 调查规格定稿

- 完成 [S1 调查契约](../../specs/product/input-reliability/spec.md)：严格 JSONL、采集容量与内存预算、跨线程关联与冻结、原始调度/时钟注入、CLI 及错误语义。
- 对照现有代码审查并同步设计、任务、测试映射与导航，记录当前用户对 TASK-001 的授权。
- TASK-001 DONE；文档治理结果见 [验证记录](../../specs/product/input-reliability/validation.md)。产品未编码或运行测试，LI-001～004 与 S4 修复门禁保持未完成。

## 2026-09-20 v0.1.1 配套文档草案（未实现）

- 新增 [输入可靠性五件套](../../specs/product/input-reliability/spec.md) 与根因调查报告框架，规划 LI-001～LI-004。
- 定义原始轨迹、调查与修复评审门禁、影响评估及真机/Headless回归计划，更新系统导航。
- 按用户要求仅编辑文档；未编码、未运行构建/测试/治理检查，未创建验证证据或签收。规格 Draft、审批 Pending、实现 NotStarted、验证 NotRun、发布 NotAccepted。

## 2026-09-20 host CI 夹具修复

- 固定测试谱面 CRLF 检出，修复 Linux 上谱面原始字节哈希与既有 Replay 不一致导致的 CTest 失败。
- 两种 Git 自动换行设置下的夹具检出与完整 Headless 结果、本地 CTest 和 SDD 检查通过；远程 sanitizer 待推送后验证。详情见 [验证记录](../../VALIDATION.md)。

## v0.1.0 正式签收

- 实现基线已提交并推送；按用户授权完成保留已知问题的正式签收，建立 v0.1.0 版本引用。
- 登记 LateInput 根因、拒绝事件分类、判定影响及修复回归为后续优先工作。


## 2026-09-20 真人补测

- 用户实际完成触摸、双指释放、暂停恢复听感与旋转验证，两轮 Replay 与 Headless 完整一致。
- 记录 LateInput 17/3 的诊断限制，未将用户“感觉正常”作为零拒绝证据；无产品代码变更。
- 证据归档，测试后断开真机连接。

## 2026-09-20 真机非人工补测

- 使用同一最终签名包完成真机 60 Hz 基线、120 Hz 请求、自动平台输入、Replay 和生命周期验证。
- 60 秒最低 55 FPS；请求 120 Hz 实际约 90 Hz，保持条件性不支持记录。
- 证据单独归档，排除真人输入复测；结束后移除 HDC 连接并确认无已连接目标。

## v0.1.0 — 2026-09-20（验收完成，未对外发布）

- 建立 C++17 内核、接口边界、ReferenceGame、Headless 与 Harmony OHAudio / XComponent / EGL Backend。
- 实现音频时钟、原始输入时间戳、BPM 映射、判定得分、Play / Replay / AutoPlay 及版本化 Replay。
- 修复真机输入正常送达却被当作 LateInput 的问题；保留原始事件时间与 50 ms 判定窗口。
- 增加 ARM64/x86_64 HAP、签名模板隔离、主机回归、架构检查、设备导出与证据归档工具。
- 主机 92 项测试和 3 组 CTest 通过；最终模拟器 60 秒基线 p95=16.6744 ms，最低 57 FPS。
- 124 原验收用例 PASS；条件性 120 Hz 用例为 UNSUPPORTED_DEVICE。真机与模拟器范围、早期失败探测和工具警告见 [验证记录](../../VALIDATION.md)。

## 2026-09-19 文档基线重建

- 按 harmony-agent-template 建立 SDD 系统文档、功能五件套和治理工具。
- 迁移完整 v0.1.0 需求及验收契约，保留原章节和 TC 编号。
- 将旧实现说明、需求原件和验收证据归档并保存哈希。
- 移除旧产品代码、第三方源码、测试夹具、CI、构建配置、脚本及产物。
- 当前产品实现 NotStarted，验证 NotRun，发布 NotAccepted。
