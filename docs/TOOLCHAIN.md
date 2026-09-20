# HRK 实现工具链

HRK 核心采用 C++17 / CMake，主机 Headless 测试不依赖 Harmony SDK、GPU 或音频设备。Harmony Native 层计划使用 OHAudio、XComponent、EGL / OpenGL ES 与 NAPI；ArkTS / ArkUI 仅提供应用外壳。当前工程位于 src/app/harmony，主机和 Native / HAP 已进行真实构建，结论见系统 VALIDATION。

本文件保留源项目工具选择和历史配置基线，不宣称是最新版本，也不代表本机已安装或通过验证。

| 层次 | 工具 / 原基线 | 用途 |
|---|---|---|
| 应用 | HarmonyOS Stage、ArkTS、ArkUI | 应用生命周期与 UI |
| IDE / SDK | DevEco Studio 6.1.1.300、HarmonyOS 6.1.1 / API 24 | 原项目构建环境 |
| 工程兼容配置 | compatibleSdkVersion 5.0.0(12)、targetSdkVersion 6.1.1(24) | 历史配置，实际兼容性须重新验证 |
| 构建 | DevEco 附带 JBR、Node、OHPM、Hvigor；modelVersion 6.0.0 | 依赖安装及 assembleHap |
| 驱动 | Python 3.9+、[harmony_build.py](../tools/harmony/harmony_build.py) | 已包含用户提供的驱动，仅使用 Python 标准库；工具链发现、OHPM 安装、Hvigor 构建 |
| 同步 | Git、OpenSSH、SSH config / 可选 ProxyJump | 源码通过版本库同步 |
| 回传 | SCP | 产物和日志回传 |
| 远端 Shell | Windows PowerShell、UTF-16LE Base64 EncodedCommand | 避免多层命令转义 |
| 治理 | Node.js 内置模块、node:test、Bash、Git、grep、sed | 无 npm 依赖的检查工具 |

通用配置样例位于 `tools/harmony/templates/`；当前产品应用位于 `src/app/harmony`。用 DevEco 创建实际工程后按环境核对或合并配置，勿覆盖 IDE 生成的必要依赖。

## 仓库内构建驱动

从仓库根目录运行：

```powershell
python tools/harmony/harmony_build.py --help
python tools/harmony/harmony_build.py --project '<project-path>' --deveco '<deveco-path>' --list --no-repair-links
python tools/harmony/harmony_build.py --project '<project-path>' --deveco '<deveco-path>'
```

参数、默认行为及验证边界见 [驱动说明](../tools/harmony/README.md)。驱动已随模板提供，DevEco SDK、Python 与真实应用工程仍须自行配置。

## 直接构建（远端 PowerShell）

填写本地变量；以下为命令模板，不自动执行。

```powershell
$ProjectPath = '<absolute-project-path>'
$DevEcoPath = '<absolute-deveco-path>'
$NodePath = Join-Path $DevEcoPath 'tools/node/node.exe'
$OhpmPath = Join-Path $DevEcoPath 'tools/ohpm/bin/ohpm.bat'
$HvigorPath = Join-Path $DevEcoPath 'tools/hvigor/bin/hvigorw.js'
$env:JAVA_HOME = Join-Path $DevEcoPath 'jbr'
$env:DEVECO_SDK_HOME = Join-Path $DevEcoPath 'sdk'
$env:PATH = (Split-Path $NodePath) + ';' + $env:PATH
Set-Location -LiteralPath $ProjectPath
& $OhpmPath install
if ($LASTEXITCODE -ne 0) { throw 'OHPM install failed' }
& $NodePath $HvigorPath --mode module -p product=default -p module=entry@default assembleHap --no-daemon
if ($LASTEXITCODE -ne 0) { throw 'Hvigor build failed' }
```

路径、product、module、任务和 SDK 必须与实际工程一致。需要签名或真机安装时按新项目规格配置，证书与口令不入库。未签名 HAP 仅能证明构建产物生成，不能证明安装、运行或功能验收。
