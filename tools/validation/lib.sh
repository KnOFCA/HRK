#!/usr/bin/env bash
# tools/validation/lib.sh —— 验证器公共库（source 使用，不直接执行）
#
# 设计约束（见 docs/migration/SDD_ARCHITECTURE_MIGRATION.md §8）：
#   1. 缺失输入绝不等于 PASS。必需的受版本控制的输入缺失 → FAIL；
#      环境 / 外部工具 / 未入库暂存区缺失 → BLOCKED。
#   2. 本库只做判定与计数，不吞错误。
#
# 调用方约定：
#   - 允许在 `set -uo pipefail` 下 source 本文件（不使用 `set -e`：验证器必须
#     继续执行以汇总全部失败项，最终由 `sdd_summary` 的退出码表达结论）。
#   - 计数器变量：SDD_PASS / SDD_FAIL / SDD_BLOCKED。

: "${SDD_PASS:=0}"
: "${SDD_FAIL:=0}"
: "${SDD_BLOCKED:=0}"

sdd_ok() {
  printf 'PASS     %s\n' "$1"
  SDD_PASS=$((SDD_PASS + 1))
}

sdd_fail() {
  printf 'FAIL     %s\n' "$1"
  SDD_FAIL=$((SDD_FAIL + 1))
}

sdd_blocked() {
  printf 'BLOCKED  %s\n' "$1"
  SDD_BLOCKED=$((SDD_BLOCKED + 1))
}

sdd_note() {
  printf 'INFO     %s\n' "$1"
}

# 受版本控制的必需输入：缺失即 FAIL（仓库缺陷）。
sdd_require_path() {
  if [ -e "$1" ]; then
    sdd_ok "input present: $1"
    return 0
  fi
  sdd_fail "MISSING INPUT (tracked path absent): $1"
  return 1
}

# 未入库（.gitignore 排除）的运行期输入：缺失即 BLOCKED，不得记为 PASS。
# 这类输入无法从版本库重建，因此不判为仓库缺陷。
sdd_require_runtime_path() {
  if [ -e "$1" ]; then
    sdd_ok "runtime input present: $1"
    return 0
  fi
  sdd_blocked "RUNTIME INPUT ABSENT (not reproducible from VCS): $1"
  return 1
}

# 外部命令依赖：缺失即 BLOCKED。
sdd_require_cmd() {
  if command -v "$1" >/dev/null 2>&1; then
    sdd_ok "dependency available: $1"
    return 0
  fi
  sdd_blocked "MISSING DEPENDENCY: $1"
  return 1
}

# 断言字符串相等；不等即 FAIL。
sdd_assert_eq() {
  local label="$1" expected="$2" actual="$3"
  if [ "$expected" = "$actual" ]; then
    sdd_ok "$label"
    return 0
  fi
  sdd_fail "$label (expected=$expected actual=$actual)"
  return 1
}

# 从标准输入逐行剥离长十六进制 token（SHA-256 / SHA-1 / MD5 等）。
# 目的：避免"哈希里的数字串被手机号正则误判"这类假阳性；
# 剥离只影响判定用的副本，不修改被检文件。
sdd_strip_hash_tokens() {
  sed -E 's/[0-9a-fA-F]{32,}//g'
}

sdd_summary() {
  printf '\n===== summary: PASS=%s FAIL=%s BLOCKED=%s =====\n' "$SDD_PASS" "$SDD_FAIL" "$SDD_BLOCKED"
  if [ "$SDD_FAIL" -gt 0 ]; then
    return 1
  fi
  if [ "$SDD_BLOCKED" -gt 0 ]; then
    return 2
  fi
  return 0
}
