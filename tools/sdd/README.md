# SDD 治理工具

`bash tools/sdd/check.sh --fast` 执行 lint、链接、证据 provenance 与公共库测试；完整入口额外检查必需文件并在隔离目录实例化三个领域的模板。
退出码：0 PASS、1 FAIL、2 BLOCKED；FAIL 优先于 BLOCKED。空规格和空证据目录合法，不代表应用验收通过。

| 工具 | 用途 |
|---|---|
| lint_sdd.mjs / lib.mjs | metadata、审批、领域与路径、需求反向依赖、ID 唯一性及引用、链接、数字求和、生命周期术语、五件套、ADR 重复、证据提交可达性 |
| new-feature.mjs | 实例化五件套并调整链接，拒绝覆盖与重复 ID |
| revision.mjs | 生成内容修订 SHA-256，避免伪造 commit |
| check-provenance.mjs / .sh | 检查当前 evidence 中的提交与引用；不依赖任何源仓库历史 |
| check.sh | 统一入口 |

lint 不是自然语言审查器：需求完整性、设计中是否重复定义契约、业务字段与模板一致性需要规格审查或功能专用验证器。不得通过放宽规则掩盖实际失败。
