#!/usr/bin/env node
// tools/sdd/lint_sdd.mjs —— 仓库级 SDD 一致性 lint
//
// 用法：
//   node tools/sdd/lint_sdd.mjs [--json]
//
// 退出码：0 = PASS（无违规）；1 = FAIL（存在违规）。
// 检查项清单见 tools/sdd/lib.mjs 头部注释与 tools/sdd/README.md。

import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { lintRepo } from './lib.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '..', '..');
const asJson = process.argv.includes('--json');

const { violations, stats, features } = lintRepo(ROOT);

if (asJson) {
  process.stdout.write(`${JSON.stringify({ violations, stats, features }, null, 2)}\n`);
} else {
  console.log('===== SDD lint =====');
  console.log(`features=${stats.features} markdown=${stats.markdown} links=${stats.linksChecked} manifests=${stats.manifests}`);
  if (violations.length === 0) {
    console.log('PASS  未发现 SDD 结构性问题');
  } else {
    for (const v of violations) {
      const loc = v.line ? `${v.file}:${v.line}` : v.file;
      console.log(`FAIL  [${v.check}] ${loc}  ${v.message}`);
    }
    console.log(`\n共 ${violations.length} 项违规`);
  }
}

process.exit(violations.length === 0 ? 0 : 1);
