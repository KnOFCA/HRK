#!/usr/bin/env bash
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.." || exit 1
source tools/validation/lib.sh
sdd_require_cmd node
sdd_require_cmd git
sdd_require_cmd bash
[ "$SDD_BLOCKED" -eq 0 ] || exit 2
export SDD_BASH="$(command -v bash)"
fails=0
blocked=0
run() {
  local label="$1"; shift
  printf '\n===== %s =====\n' "$label"
  "$@"
  local rc=$?
  case "$rc" in
    0) ;;
    2) blocked=$((blocked + 1));;
    *) fails=$((fails + 1));;
  esac
}
run 'SDD lint' node tools/sdd/lint_sdd.mjs
run 'Markdown links' node tools/validation/check-links.mjs
run 'Evidence provenance' bash tools/sdd/check-provenance.sh
run 'Validator tests' node --test tools/validation/tests/validators.test.mjs
if [ "${1:-}" != '--fast' ]; then
  run 'Required inputs' bash tools/validation/verify.sh
  run 'Template dry-run' bash tools/validation/dryrun.sh
fi
printf '\n===== FAIL=%s BLOCKED=%s =====\n' "$fails" "$blocked"
[ "$fails" -eq 0 ] || exit 1
[ "$blocked" -eq 0 ] || exit 2
exit 0
