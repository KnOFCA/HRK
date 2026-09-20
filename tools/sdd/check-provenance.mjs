#!/usr/bin/env node
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { lintRepo } from './lib.mjs';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const git = spawnSync('git', ['--version'], { encoding: 'utf8' });
if (git.error) { console.error('BLOCKED: git unavailable'); process.exit(2); }
const result = lintRepo(root);
const failures = result.violations.filter(v => v.check === 'C6-provenance');
for (const v of failures) console.error(`FAIL ${v.file}: ${v.message}`);
console.log(`Evidence manifests=${result.stats.manifests}; failures=${failures.length}`);
if (!result.stats.manifests) console.log('INFO: no evidence yet; no application validation claimed.');
process.exit(failures.length ? 1 : 0);
