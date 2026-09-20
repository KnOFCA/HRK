# 验证证据

仅在真实执行后创建 `<FEATURE-ID>/<run-id>/manifest.json` 与校验和文件。模板不继承任何旧提交、tag、运行日志或 PASS 结论。

manifest 应记录：feature_id、run_id、实际时间与环境、specification_revision、design_revision、implementation_commit、validator_commit 或 validator_revision、validation_commit、validation_ref、命令与退出码、逐条 AC 结果、产物路径及 SHA-256。
未提交的内容用 `node tools/sdd/revision.mjs tools` 等内容哈希表示，不填伪造 commit。正式提交必须存在且可达；稳定引用可采用 `refs/tags/validation/<feature>/<run-id>`。敏感日志和构建产物存放在被忽略的 build/，这里只保留脱敏摘要与哈希。
