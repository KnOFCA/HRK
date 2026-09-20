// tools/validation/lib.mjs —— 验证器可复用逻辑（被 CLI 与单元测试共同引用）
//
// 与 tools/validation/lib.sh 同一契约：
//   缺失输入 != PASS。显式传入的目标文件不存在 → missingInputs（FAIL）；
//   文档内链接指向不存在的相对路径 → broken（FAIL）。

import fs from 'node:fs';
import path from 'node:path';

export const SKIP_DIRS = new Set(['.git', 'node_modules', 'build', '.codeartsdoer']);

/** 递归收集仓库内的 Markdown 文件（相对 root 的 POSIX 路径）。 */
export function collectMarkdown(root, dir = '.') {
  const abs = path.resolve(root, dir);
  const out = [];
  for (const entry of fs.readdirSync(abs, { withFileTypes: true })) {
    if (entry.isDirectory()) {
      if (SKIP_DIRS.has(entry.name)) continue;
      out.push(...collectMarkdown(root, path.posix.join(dir, entry.name)));
    } else if (entry.isFile() && entry.name.toLowerCase().endsWith('.md')) {
      out.push(path.posix.normalize(path.posix.join(dir, entry.name)));
    }
  }
  return out.sort();
}

/** 提取 Markdown 内联链接的目标；忽略外部 URL、纯锚点与 mailto。 */
export function extractLinks(text) {
  const out = [];
  const re = /\[[^\]]*\]\(([^)]+)\)/g;
  let m;
  while ((m = re.exec(text)) !== null) {
    let target = m[1].trim();
    if (/^(https?:|mailto:|#|data:)/i.test(target)) continue;
    if (target.startsWith('<') && target.endsWith('>')) target = target.slice(1, -1);
    target = target.split('#')[0].split('?')[0];
    if (!target) continue;
    try {
      target = decodeURIComponent(target);
    } catch {
      /* 保留原样 */
    }
    out.push(target);
  }
  return out;
}

/**
 * 检查给定文件集合中的相对链接。
 * @returns {{checked:number, broken:Array, missingInputs:Array, skipped:number}}
 */
export function checkLinks(root, files) {
  const broken = [];
  const missingInputs = [];
  let checked = 0;
  let skipped = 0;

  for (const rel of files) {
    const abs = path.resolve(root, rel);
    if (!fs.existsSync(abs) || !fs.statSync(abs).isFile()) {
      // 显式传入的输入缺失 —— 绝不当作 PASS / SKIP。
      missingInputs.push(rel);
      continue;
    }
    const text = fs.readFileSync(abs, 'utf8');
    for (const target of extractLinks(text)) {
      const resolved = path.resolve(path.dirname(abs), target);
      checked += 1;
      if (!fs.existsSync(resolved)) {
        broken.push({ file: rel, target });
      } else {
        skipped += 0;
      }
    }
  }
  return { checked, broken, missingInputs, skipped };
}
