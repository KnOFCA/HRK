#!/usr/bin/env node
// tools/sdd/revision.mjs —— 计算目录/文件的内容修订号（确定性哈希）
//
// 用途：Validation 证据需要指向"验证器版本"。当改动尚未提交为 Git commit 时，
// 用 tools/ 目录的内容哈希（validator_revision）表达版本，避免伪造 commit。
//
// 用法：
//   node tools/sdd/revision.mjs [path]        # 默认 tools
//
// 输出：<sha256>  <path>  (files=<n>)
// 算法：对目录内所有文件按相对路径排序，逐个拼接 `<relpath>\0<file-sha256>\n` 后取 SHA-256。

import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '..', '..');
const target = process.argv[2] || 'tools';
const abs = path.resolve(ROOT, target);
if (!fs.existsSync(abs)) {
  console.error(`MISSING-INPUT  ${target}`);
  process.exit(2);
}

function walk(dir, base, out) {
  const entries = fs.readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name));
  for (const e of entries) {
    const p = path.join(dir, e.name);
    const rel = path.posix.join(base, e.name);
    if (e.isDirectory()) walk(p, rel, out);
    else if (e.isFile()) out.push(rel);
  }
  return out;
}

const files = fs.statSync(abs).isFile() ? [path.posix.basename(abs)] : walk(abs, '', []);
const h = crypto.createHash('sha256');
for (const rel of files) {
  const full = fs.statSync(abs).isFile() ? abs : path.join(abs, rel);
  const fh = crypto.createHash('sha256').update(fs.readFileSync(full)).digest('hex');
  h.update(`${rel}\u0000${fh}\n`);
}
process.stdout.write(`${h.digest('hex')}  ${target}  (files=${files.length})\n`);
