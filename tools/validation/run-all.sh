#!/usr/bin/env bash
set -uo pipefail
exec bash "$(dirname "${BASH_SOURCE[0]}")/../sdd/check.sh" "$@"
