#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { DOMAINS, FEATURE_DOCS, discoverFeatureDirs, parseFrontMatter } from './lib.mjs';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const [domain, name, id] = process.argv.slice(2);
const prefixes = { product: 'PROD-', engineering: 'ENG-', process: 'PROC-' };
if (!DOMAINS.includes(domain) || !/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(name || '') ||
    !/^[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)+$/.test(id || '') || !id.startsWith(prefixes[domain])) {
  console.error('Usage: node tools/sdd/new-feature.mjs <product|engineering|process> <kebab-name> <PROD/ENG/PROC-NAME>');
  process.exit(1);
}
const dest = path.join(root, 'specs', domain, name);
if (fs.existsSync(dest)) { console.error('FAIL: target already exists'); process.exit(1); }
for (const feature of discoverFeatureDirs(root)) {
  const file = path.join(root, feature.dir, 'spec.md');
  if (fs.existsSync(file) && parseFrontMatter(fs.readFileSync(file, 'utf8')).data?.id === id) {
    console.error('FAIL: feature ID already exists'); process.exit(1);
  }
}
// Read all inputs before creating a destination; missing template must fail.
const docs = FEATURE_DOCS.map(doc => [doc, fs.readFileSync(path.join(root, 'specs/_template', doc), 'utf8')
  .replaceAll('<DOMAIN-FEATURE>', id).replaceAll('<功能名称>', name)
  .replaceAll('<product|engineering|process>', domain)
  .replaceAll('specs/<domain>/<feature-name>/spec.md', `specs/${domain}/${name}/spec.md`)
  .replace(/\]\(\.\.\/\.\.\//g, '](../../../')
  .replaceAll('](../README.md)', '](../../README.md)')]);
fs.mkdirSync(dest, { recursive: true });
for (const [doc, content] of docs) fs.writeFileSync(path.join(dest, doc), content);
console.log(`Created specs/${domain}/${name}; fill TBD, review and record actual authorization before implementation.`);
