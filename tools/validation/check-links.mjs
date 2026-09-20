#!/usr/bin/env node
// tools/validation/check-links.mjs —— 仓库内 Markdown 相对链接校验
//
// 用法：
//   node tools/validation/check-links.mjs                # 扫描全仓库 Markdown
//   node tools/validation/check-links.mjs <file...>      # 只检查给定文件
//   node tools/validation/check-links.mjs --json         # 机器可读输出
//
// 退出码：0 = PASS（无失效链接）；1 = FAIL（存在失效链接）；2 = FAIL（显式输入缺失）。

import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { checkLinks, collectMarkdown } from './lib.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '..', '..');

const argv = process.argv.slice(2);
const asJson = argv.includes('--json');
const targets = argv.filter((a) => !a.startsWith('--'));

const files = targets.length ? targets : collectMarkdown(ROOT);
const result = checkLinks(ROOT, files);

if (asJson) {
  process.stdout.write(`${JSON.stringify(result, null, 2)}\n`);
} else {
  for (const rel of result.missingInputs) {
    console.log(`MISSING-INPUT  ${rel}`);
  }
  for (const { file, target } of result.broken) {
    console.log(`BROKEN  ${file}  ->  ${target}`);
  }
  console.log(
    `\n链接检查: 已检查=${result.checked} 失效=${result.broken.length} 输入缺失=${result.missingInputs.length}`,
  );
}

if (result.missingInputs.length > 0) process.exit(2);
if (result.broken.length > 0) process.exit(1);
process.exit(0);
