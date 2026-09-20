// Synthetic isolated workspace: tests template generation, links, duplicate protection and lint rejection.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { lintRepo } from '../sdd/lib.mjs';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const base = path.join(root, 'build');
fs.mkdirSync(base, { recursive: true });
const temp = fs.mkdtempSync(path.join(base, 'template-dryrun-'));
try {
  for (const name of fs.readdirSync(root)) {
    if (['.git', 'build', 'node_modules'].includes(name)) continue;
    fs.cpSync(path.join(root, name), path.join(temp, name), { recursive: true });
  }
  const generate = (...args) => spawnSync(process.execPath, ['tools/sdd/new-feature.mjs', ...args], { cwd: temp, encoding: 'utf8' });
  for (const [domain, id] of [['product', 'PROD'], ['engineering', 'ENG'], ['process', 'PROC']]) {
    const result = generate(domain, 'fixture', `${id}-FIXTURE`);
    assert.equal(result.status, 0, result.stderr);
  }
  assert.deepEqual(lintRepo(temp).violations, []);
  assert.equal(generate('product', 'fixture', 'PROD-FIXTURE').status, 1);
  assert.equal(generate('product', 'another', 'PROD-FIXTURE').status, 1);
  assert.equal(generate('product', '../escape', 'PROD-ESCAPE').status, 1);
  const spec = path.join(temp, 'specs/product/fixture/spec.md');
  fs.writeFileSync(spec, fs.readFileSync(spec, 'utf8').replace('spec_status: Draft', 'spec_status: Approved'));
  assert.ok(lintRepo(temp).violations.some(v => v.check === 'C1-approval'));
  console.log('PASS: all three domains instantiate; links/IDs valid; duplicates, traversal and forged approval rejected.');
} finally {
  // mkdtemp creates a unique directory under the known workspace build root.
  if (path.dirname(temp) !== base) throw new Error('Unexpected cleanup path');
  fs.rmSync(temp, { recursive: true, force: true });
}
