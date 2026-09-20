# HarmonyOS 远程构建与产物回传

这是可复用操作参考。采用前将工程要求与验收条件登记到自己的 engineering 规格；步骤不自动获得执行或发布授权。

## 1. 配置

| 参数 | 填写内容 |
|---|---|
| SSH_CONFIG / REMOTE | 本地 SSH 配置文件、远端别名；可选跳板机 |
| LOCAL_REPO / REMOTE_REPO | 本地及远端仓库绝对路径 |
| BUILD_COMMIT / BUILD_REF | 本次已推送的真实提交 SHA 及可访问引用 |
| PROJECT_PATH / PROJECT_REL | 远端工程路径 / 仓库内相对路径 |
| DRIVER_PATH / DEVECO_PATH | 远端仓库 tools/harmony/harmony_build.py 路径 / DevEco 路径 |
| ARTIFACT_SOURCE / ARTIFACT_DEST | 远端产物路径 / 本地 build/artifacts-from-remote 下目录 |

不保存账号、主机地址、私钥或个人目录。先验证 SSH 连通、工具链与工程文件在位；外部依赖缺失记录 BLOCKED。

## 2. 基线与源码同步

记录本地和远端的 HEAD、分支、git status、必要的文件哈希。工作区不干净时先隔离或处理，不强制覆盖。
源码经 Git commit / push / fetch 同步，核对远端 SHA 与本地 BUILD_COMMIT 完全一致。远端不直接修改业务代码。
优先使用独立构建检出目录；需要切换已有检出时先记录原状态，完成后恢复并复核。只有本次创建且确认不再需要的临时分支才进入清理流程。

## 3. PowerShell 传输

多行远程命令采用 UTF-16LE Base64 编码，避免 SSH 与 PowerShell 双重转义。以下在本地 PowerShell 执行：

```powershell
$SshConfig = '<ssh-config-path>'
$RemoteAlias = '<ssh-alias>'
$ScriptPath = '<local-powershell-script-path>'
$ScriptText = Get-Content -LiteralPath $ScriptPath -Raw -Encoding UTF8
$Encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($ScriptText))
ssh -F $SshConfig -o BatchMode=yes $RemoteAlias "powershell -NoProfile -NonInteractive -EncodedCommand $Encoded"
if ($LASTEXITCODE -ne 0) { throw 'Remote command failed' }
```

远端脚本设置 `$ErrorActionPreference = 'Stop'`，显式检查每个原生命令退出码。允许网络和初次构建拥有足够超时，超时必须明确记录，不能把截断输出当作成功。

## 4. 发现与构建

驱动随 Git 同步到远端仓库，在远端 PowerShell 执行：

```powershell
$RemoteRepo = '<absolute-remote-repository-path>'
$DriverPath = Join-Path $RemoteRepo 'tools/harmony/harmony_build.py'
$ProjectPath = '<absolute-project-path>'
python $DriverPath --project $ProjectPath --list --no-repair-links
if ($LASTEXITCODE -ne 0) { throw 'Toolchain discovery failed' }
python $DriverPath --project $ProjectPath
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
```

驱动支持 `--project`、`--list`、`--deveco` 等参数，详见 [驱动说明](../../../tools/harmony/README.md)。也可使用 [OHPM/Hvigor 直接命令](../../TOOLCHAIN.md)。只读探测须加 `--no-repair-links`，因为链接预检发生在 `--list` / `--dry-run` 返回之前。日志须保留完整输出及真实退出码；驱动即使未发现产物也可能返回 0，并会列出已有产物，因此必须独立检查本次产物及哈希。

## 5. 产物回传与校验

根据本次构建实际输出列表，使用 SCP 回传 HAP/HSP/APP/HAR、pack.info、需要的 source map、Hvigor 日志与驱动输出；不假定所有类型都会生成。

```powershell
$RemoteArtifact = '<ssh-alias>:<remote-artifact-path>'
$ArtifactDestination = '<local-build-artifacts-path>'
scp -F $SshConfig -r $RemoteArtifact $ArtifactDestination
if ($LASTEXITCODE -ne 0) { throw 'Artifact transfer failed' }
Get-FileHash -LiteralPath '<local-artifact-file>' -Algorithm SHA256
```

远端也使用 Get-FileHash，逐文件比较 SHA-256、大小与清单，确认来自本次构建。产物放在 build/ 并忽略入库。源码通过 Git、产物通过 SCP。

## 6. 恢复与记录

恢复实际修改的现场，比较 HEAD、分支、工作区状态、原有文件内容与必要哈希，不能只比较 git status。不要运行无范围限制的清理命令。
将环境、源码提交、工具版本、命令、退出码、输出摘要、哈希、恢复确认写入功能 validation.md 与 [evidence](../../../evidence/README.md)。失败分类沿用开发协议；未签名构建、设备运行、产品验收分别记录。
