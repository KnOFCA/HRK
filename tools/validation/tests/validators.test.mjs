// tools/validation/tests/validators.test.mjs —— 验证器自测
//
// 覆盖要求（见 docs/migration/SDD_ARCHITECTURE_MIGRATION.md §10）：
//   valid input        → PASS
//   invalid input      → FAIL
//   missing file       → FAIL / BLOCKED
//   wrong path         → FAIL / BLOCKED
//   missing dependency → BLOCKED
//
// 运行：node --test tools/validation/tests/validators.test.mjs

import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync, spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { checkLinks } from '../lib.mjs';
import { lintRepo } from '../../sdd/lib.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '..', '..', '..');
const TMP_ROOT = path.join(ROOT, 'build', 'sdd-test-tmp');
const BASH = process.env.SDD_BASH || 'bash';

let counter = 0;
function makeTmpDir() {
  counter += 1;
  const dir = path.join(TMP_ROOT, `case-${process.pid}-${counter}`);
  fs.rmSync(dir, { recursive: true, force: true });
  fs.mkdirSync(dir, { recursive: true });
  return dir;
}

function write(dir, rel, content) {
  const abs = path.join(dir, rel);
  fs.mkdirSync(path.dirname(abs), { recursive: true });
  fs.writeFileSync(abs, content);
}

function fixture() {
  const dir = makeTmpDir();
  write(
    dir,
    'specs/process/demo/spec.md',
    `---
id: DEMO-FEAT
legacy_ids: [FEAT-999]
title: Demo
domain: process
spec_status: Reviewed
approval: Pending
approver: null
approval_date: null
approval_evidence: null
implementation_status: Complete
validation_status: PASS
release_status: NotAccepted
canonical_spec: specs/process/demo/spec.md
requirements_version: 1
required_fields: 12
additional_fields: 5
total_fields: 17
---

# Demo spec

本规格区分 Workflow Step / Validation Gate / Recording Step 三个概念。

| 需求编号 | 需求描述 | 优先级 | 验收标准编号 |
|---|---|---|---|
| DEMO-FEAT-REQ-001 | 系统 SHALL 做某事 | P0 | DEMO-FEAT-AC-001 |

| 编号 | 对应需求 | 验收标准 |
|---|---|---|
| DEMO-FEAT-AC-001 | DEMO-FEAT-REQ-001 | 做某事成功 |
`,
  );
  write(dir, 'specs/process/demo/design.md', '# Demo design\n\n本设计实现 DEMO-FEAT-REQ-001。\n');
  write(dir, 'specs/process/demo/tasks.md', '# Demo tasks\n\n| 任务 | 规格 |\n|---|---|\n| DEMO-FEAT-TASK-001 | DEMO-FEAT-REQ-001 |\n');
  write(dir, 'specs/process/demo/test-plan.md', '# Demo test plan\n\n| 测试 | 规格 |\n|---|---|\n| DEMO-FEAT-TEST-001 | DEMO-FEAT-AC-001 |\n');
  write(dir, 'specs/process/demo/validation.md', '# Demo validation\n\n结果：PASS（DEMO-FEAT-AC-001）。\n');
  write(
    dir,
    'docs/migration/SDD_ARCHITECTURE_MIGRATION.md',
    '# Migration\n\n| Legacy ID | Canonical ID |\n|---|---|\n| FEAT-999 | DEMO-FEAT |\n',
  );
  return dir;
}

function checksFor(root) {
  return lintRepo(root).violations.map((v) => v.check);
}

test('checkLinks: valid input passes', () => {
  const dir = makeTmpDir();
  write(dir, 'a.md', 'see [b](b.md)');
  write(dir, 'b.md', '# b');
  const r = checkLinks(dir, ['a.md']);
  assert.equal(r.broken.length, 0);
  assert.equal(r.missingInputs.length, 0);
  assert.equal(r.checked, 1);
});

test('checkLinks: broken link fails', () => {
  const dir = makeTmpDir();
  write(dir, 'a.md', 'see [missing](nope.md)');
  const r = checkLinks(dir, ['a.md']);
  assert.equal(r.broken.length, 1);
  assert.equal(r.broken[0].target, 'nope.md');
});

test('checkLinks: missing input file fails (never silently skipped)', () => {
  const dir = makeTmpDir();
  const r = checkLinks(dir, ['does-not-exist.md']);
  assert.deepEqual(r.missingInputs, ['does-not-exist.md']);
  assert.equal(r.checked, 0);
});

test('checkLinks: wrong path fails', () => {
  const dir = makeTmpDir();
  write(dir, 'docs/a.md', 'see [x](../../outside/x.md)');
  const r = checkLinks(dir, ['docs/a.md']);
  assert.equal(r.broken.length, 1);
});

test('lint: valid fixture has no violations', () => {
  const dir = fixture();
  assert.deepEqual(checksFor(dir), []);
});

test('lint: invalid status is rejected', () => {
  const dir = fixture();
  const p = path.join(dir, 'specs/process/demo/spec.md');
  fs.writeFileSync(p, fs.readFileSync(p, 'utf8').replace('spec_status: Reviewed', 'spec_status: Delivered'));
  assert.ok(checksFor(dir).includes('C1-status'));
});

test('lint: missing required metadata is rejected', () => {
  const dir = fixture();
  const p = path.join(dir, 'specs/process/demo/spec.md');
  fs.writeFileSync(p, fs.readFileSync(p, 'utf8').replace('release_status: NotAccepted\n', ''));
  assert.ok(checksFor(dir).includes('C1-metadata'));
});

test('lint: forged Approved without approver is rejected', () => {
  const dir = fixture();
  const p = path.join(dir, 'specs/process/demo/spec.md');
  fs.writeFileSync(
    p,
    fs
      .readFileSync(p, 'utf8')
      .replace('spec_status: Reviewed', 'spec_status: Approved')
      .replace('approval: Pending', 'approval: Approved'),
  );
  const checks = checksFor(dir);
  assert.ok(checks.includes('C1-approval'));
});

test('lint: duplicate canonical ID is rejected', () => {
  // 同一文件内的重复出现视为映射/引用表（例如 §7 可追溯性表重复列出 requirement ID），
  // 只有跨 feature 文件的重复定义才是冲突。
  const dir = fixture();
  write(
    dir,
    'specs/process/demo2/spec.md',
    `---
id: DEMO-FEAT2
legacy_ids: []
title: Demo2
domain: process
spec_status: Reviewed
approval: Pending
approver: null
approval_date: null
approval_evidence: null
implementation_status: NotStarted
validation_status: NotRun
release_status: NotAccepted
canonical_spec: specs/process/demo2/spec.md
requirements_version: 1
---

# Demo2 spec

Workflow Step / Validation Gate / Recording Step 术语。

| 编号 | 对应需求 | 验收标准 |
|---|---|---|
| DEMO-FEAT-AC-001 | DEMO-FEAT-REQ-001 | 与 demo 冲突的 ID |
`,
  );
  assert.ok(checksFor(dir).includes('C3-duplicate-id'));
});

test('lint: numeric inconsistency is rejected', () => {
  const dir = fixture();
  const p = path.join(dir, 'specs/process/demo/spec.md');
  fs.writeFileSync(p, fs.readFileSync(p, 'utf8').replace('total_fields: 17', 'total_fields: 16'));
  assert.ok(checksFor(dir).includes('C7-numeric'));
});

test('lint: SPEC->DESIGN requirement dependency is rejected', () => {
  const dir = fixture();
  const p = path.join(dir, 'specs/process/demo/spec.md');
  fs.writeFileSync(
    p,
    fs
      .readFileSync(p, 'utf8')
      .replace('| 做某事成功 |', '| 列名与顺序与 design.md §5.1 一致 |'),
  );
  assert.ok(checksFor(dir).includes('C2-reverse-dependency'));
});

test('lint: unexplained canonical ID reference is rejected', () => {
  const dir = fixture();
  write(dir, 'specs/process/demo/tasks.md', '# tasks\n\n见 DEMO-FEAT-REQ-404。\n');
  assert.ok(checksFor(dir).includes('C3b-reference'));
});

test('lint: missing feature document is rejected', () => {
  const dir = fixture();
  fs.rmSync(path.join(dir, 'specs/process/demo/test-plan.md'));
  assert.ok(checksFor(dir).includes('C10-feature-docs'));
});

test('shell lib: missing dependency yields BLOCKED (exit 2)', () => {
  const script = 'source tools/validation/lib.sh; sdd_require_cmd definitely-not-a-real-command-xyz; sdd_summary';
  let code = 0;
  try {
    execFileSync(BASH, ['-c', script], {
      cwd: ROOT,
      env: { ...process.env, PATH: '/nonexistent-path-for-test' },
      stdio: 'pipe',
    });
  } catch (err) {
    code = err.status;
  }
  assert.equal(code, 2, 'missing dependency must be BLOCKED, not PASS');
});

test('shell lib: present dependency yields PASS (exit 0)', () => {
  const script = 'source tools/validation/lib.sh; sdd_require_cmd bash; sdd_summary';
  execFileSync(BASH, ['-c', script], { cwd: ROOT, stdio: 'pipe' });
});

test('shell lib: missing tracked path yields FAIL (exit 1)', () => {
  const script = 'source tools/validation/lib.sh; sdd_require_path no/such/tracked/file.md; sdd_summary';
  let code = 0;
  try {
    execFileSync(BASH, ['-c', script], { cwd: ROOT, stdio: 'pipe' });
  } catch (err) {
    code = err.status;
  }
  assert.equal(code, 1, 'missing tracked input must be FAIL');
});

test('shell lib: absent runtime path yields BLOCKED (exit 2)', () => {
  const script = 'source tools/validation/lib.sh; sdd_require_runtime_path build/no-such-runtime-input; sdd_summary';
  let code = 0;
  try {
    execFileSync(BASH, ['-c', script], { cwd: ROOT, stdio: 'pipe' });
  } catch (err) {
    code = err.status;
  }
  assert.equal(code, 2, 'absent gitignored runtime input must be BLOCKED');
});

test('shell lib: sha256 hex tokens do not trigger the phone-number pattern', () => {
  const script =
    "source tools/validation/lib.sh; printf '%s\\n' '| x | c0a314256730167a7ae777e7f7d05e9ce17c480ff60468fe4fd0aaa66ff87714 |' | sdd_strip_hash_tokens | grep -qE '1[3-9][0-9]{9}' && echo HIT || echo CLEAN";
  const out = execFileSync(BASH, ['-c', script], { cwd: ROOT, encoding: 'utf8' });
  assert.equal(out.trim(), 'CLEAN', 'SHA-256 digits must not be reported as a phone number');
});

test('shell lib: real phone-like token is still detected', () => {
  const script =
    "source tools/validation/lib.sh; printf '%s\\n' 'call 13812345678 now' | sdd_strip_hash_tokens | grep -qE '1[3-9][0-9]{9}' && echo HIT || echo CLEAN";
  const out = execFileSync(BASH, ['-c', script], { cwd: ROOT, encoding: 'utf8' });
  assert.equal(out.trim(), 'HIT', 'a real phone number must still be detected');
});

test('CLI: check-links.mjs exits 2 on missing explicit input', () => {
  let code = 0;
  try {
    execFileSync(process.execPath, ['tools/validation/check-links.mjs', 'no/such/file.md'], {
      cwd: ROOT,
      stdio: 'pipe',
    });
  } catch (err) {
    code = err.status;
  }
  assert.equal(code, 2);
});

test('CLI: lint_sdd.mjs runs and reports on the real repository', () => {
  const res = spawnSync(process.execPath, ['tools/sdd/lint_sdd.mjs', '--json'], {
    cwd: ROOT,
    encoding: 'utf8',
  });
  // 退出码 0（无违规）或 1（有违规）都算"lint 可运行"；崩溃 / 语法错误不算。
  assert.ok([0, 1].includes(res.status), `unexpected exit status ${res.status}: ${res.stderr}`);
  const parsed = JSON.parse(res.stdout);
  assert.equal(res.status, 0, res.stdout);
  assert.ok(Number.isInteger(parsed.stats.features));
  assert.ok(Array.isArray(parsed.violations));
});

test('shell scripts: bash syntax check', () => {
  for (const rel of [
    'tools/validation/lib.sh',
    'tools/validation/verify.sh',
    'tools/validation/dryrun.sh',
    'tools/validation/run-all.sh',
    'tools/sdd/check.sh',
    'tools/sdd/check-provenance.sh',
  ]) {
    execFileSync(BASH, ['-n', rel], { cwd: ROOT, stdio: 'pipe' });
  }
});

test.after(() => {
  fs.rmSync(TMP_ROOT, { recursive: true, force: true });
});
