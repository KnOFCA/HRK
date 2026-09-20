# HarmonyOS 构建工具

[harmony_build.py](harmony_build.py) 为用户提供的原始脚本，按字节原样保存。仅依赖 Python 标准库；建议 Python 3.9+。实际构建还需要 DevEco Studio 的 JBR、SDK、Node、OHPM、Hvigor 及真实 HarmonyOS 工程。

## 使用

从仓库根目录执行，路径替换为实际值：

```powershell
python tools/harmony/harmony_build.py --help
python tools/harmony/harmony_build.py --project '<project-path>' --deveco '<deveco-path>' --list --no-repair-links
python tools/harmony/harmony_build.py --project '<project-path>' --deveco '<deveco-path>' --dry-run --no-repair-links
python tools/harmony/harmony_build.py --project '<project-path>' --deveco '<deveco-path>' --module entry@default --task assembleHap
```

| 参数 | 行为 |
|---|---|
| --project PATH | 含 hvigorfile.ts 和 build-profile.json5 的工程根目录；可用 HARMONY_PROJECT 或自动向上查找 |
| --deveco PATH | DevEco 根目录；也支持 DEVECO_HOME / DEVECO_STUDIO_HOME / DEVECO_STUDIO_PATH 等环境变量及常见路径探测 |
| --product NAME | 默认选择 profile 中首个 product |
| --module NAME[@TARGET] | 可重复；默认选择 profile 中全部模块及各自首个 target |
| --task NAME | 可重复；默认 assembleHap |
| --mode module/project | 默认 module |
| --build-mode debug/release | 传递 buildMode |
| --clean | 构建前执行 Hvigor clean |
| --skip-install | 跳过默认 OHPM install |
| --daemon | 启用 daemon；默认 --no-daemon |
| --no-repair-links | 禁用默认的 Windows oh_modules 不可遍历链接自动重建 |
| --list / --dry-run | 打印配置和命令，跳过安装与构建；仍会执行链接预检 |

## 行为边界

- 只读探测同时指定 `--no-repair-links`。默认链接修复会重建检测到的 oh_modules 目录链接。
- profile 解析仅支持 JSON 及部分 JSON5（注释、尾逗号），不支持完整 JSON5；解析失败可能退回默认 product / 模块。使用 --list 核对实际选择。
- 驱动返回 2 表示工具链或工程发现失败；构建命令失败时透传非零退出码，无法启动命令返回 127，中断返回 130。构建命令通过时返回 0。
- 返回 0 不保证本次产物存在或新鲜：即使未找到产物也会输出 BUILD SUCCEEDED，且产物扫描可能包含旧文件。按 [远程工作流](../../docs/workflow/remote-build/WORKFLOW.md) 独立核对。
- 驱动输出到控制台，不自动生成归档日志或证据清单；由调用方保存。

`templates/` 保留 Hvigor 与构建配置参考，不是完整应用。版本基线见 [工具链](../../docs/TOOLCHAIN.md)。
