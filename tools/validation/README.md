# 验证工具

公共库 lib.sh 统一 PASS / FAIL / BLOCKED；lib.mjs 与 check-links.mjs 检查 Markdown 相对路径。tests/validators.test.mjs 验证有效、无效、缺失输入和外部依赖处理。
verify.sh 检查框架必需文件；dryrun.sh / dryrun.mjs 在 build/ 的独立目录演练真实模板实例化及拒绝错误的能力；run-all.sh 转发完整 SDD 检查。

受版本控制的必需输入缺失为 FAIL；外部命令或运行期输入缺失为 BLOCKED。check-links CLI 对缺失显式输入保留源工具的退出码 2，但语义是输入失败，不能解释为链接已通过；统一入口不传显式文件参数。
治理检查不调用 SSH、不构建应用、不操作远端分支、不发布。产品测试根据新规格自行添加。
