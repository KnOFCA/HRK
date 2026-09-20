#!/usr/bin/env bash
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.." || exit 1
source tools/validation/lib.sh
for file in AGENTS.md agent.md CONTRIBUTING.md README.md SPEC.md DESIGN.md TASKS.md TEST_PLAN.md VALIDATION.md \
  .gitignore .gitattributes docs/TOOLCHAIN.md docs/EXTRACTION.md docs/adr/README.md \
  docs/changes/CHANGELOG.md docs/migration/SDD_ARCHITECTURE_MIGRATION.md \
  docs/workflow/remote-build/WORKFLOW.md specs/README.md \
  specs/product/README.md specs/engineering/README.md specs/process/README.md \
  specs/_template/spec.md specs/_template/design.md specs/_template/tasks.md \
  specs/_template/test-plan.md specs/_template/validation.md src/README.md tests/README.md evidence/README.md \
  tools/harmony/harmony_build.py tools/harmony/README.md \
  tools/sdd/lib.mjs tools/sdd/lint_sdd.mjs tools/sdd/revision.mjs tools/sdd/new-feature.mjs \
  tools/sdd/check.sh tools/sdd/check-provenance.sh tools/sdd/check-provenance.mjs \
  tools/validation/lib.sh tools/validation/lib.mjs tools/validation/check-links.mjs \
  tools/validation/tests/validators.test.mjs tools/validation/dryrun.mjs; do
  sdd_require_path "$file"
done
sdd_summary
