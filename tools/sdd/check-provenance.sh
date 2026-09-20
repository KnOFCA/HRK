#!/usr/bin/env bash
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.." || exit 1
source tools/validation/lib.sh
sdd_require_cmd node || exit 2
sdd_require_cmd git || exit 2
node tools/sdd/check-provenance.mjs
