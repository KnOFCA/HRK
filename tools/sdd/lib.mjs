// tools/sdd/lib.mjs —— SDD 一致性 lint 引擎（被 CLI 与单元测试共同引用）
//
// 目标不是理解自然语言，而是捕获高价值的结构性漂移：
//   C1  状态合法性 + machine-readable metadata 完整性
//   C2  禁止 SPEC 依赖 DESIGN 定义 requirement（反向依赖）
//   C3  canonical / legacy ID 唯一性
//   C4  相对链接失效
//   C5  验证脚本声明的仓库路径必须存在
//   C6  Validation 所引用的 Git commit 必须可达（稳定引用）
//   C7  结构化数字一致性（required + additional == total）
//   C8  生命周期状态不得使用模糊的单一状态词（Delivered / 已交付）
//   C9  每个 feature 规格必须定义 step / gate / recording 概念
//   C10 feature 目录必须含 spec/design/tasks/test-plan/validation 五件套
//   DESIGN 不得重新定义 requirement（通过规格审查保证）
//   C12 ADR 定义必须唯一存放在 docs/adr/

import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { checkLinks, collectMarkdown } from '../validation/lib.mjs';

export const SPEC_STATUSES = ['Draft', 'Ready-for-Review', 'Reviewed', 'Approved', 'Superseded'];
export const APPROVAL_STATUSES = ['Pending', 'Approved'];
export const IMPLEMENTATION_STATUSES = ['NotStarted', 'InProgress', 'Complete'];
export const VALIDATION_STATUSES = ['NotRun', 'PASS', 'FAIL', 'BLOCKED'];
export const RELEASE_STATUSES = ['NotAccepted', 'Accepted', 'Released', 'Superseded'];
export const DOMAINS = ['product', 'engineering', 'process'];

export const REQUIRED_METADATA = [
  'id',
  'legacy_ids',
  'title',
  'domain',
  'spec_status',
  'approval',
  'approver',
  'approval_date',
  'approval_evidence',
  'implementation_status',
  'validation_status',
  'release_status',
  'canonical_spec',
  'requirements_version',
];

export const FEATURE_DOCS = ['spec.md', 'design.md', 'tasks.md', 'test-plan.md', 'validation.md'];

const CANONICAL_ID_RE = /\b([A-Z][A-Z0-9]*(?:-[A-Z0-9]+)*-(?:REQ|AC|TASK|TEST)-\d{3})\b/g;

function read(root, rel) {
  return fs.readFileSync(path.resolve(root, rel), 'utf8');
}

function exists(root, rel) {
  return fs.existsSync(path.resolve(root, rel));
}

// ---------------------------------------------------------------- front matter

/** 解析 spec.md 顶部的 YAML front matter（仅支持本仓库使用的标量子集）。 */
export function parseFrontMatter(text) {
  const lines = text.split(/\r?\n/);
  if (lines[0]?.trim() !== '---') return { data: null, bodyStart: 0 };
  const data = {};
  let i = 1;
  for (; i < lines.length; i += 1) {
    const line = lines[i];
    if (line.trim() === '---') break;
    if (!line.trim() || line.trim().startsWith('#')) continue;
    const m = /^([A-Za-z_][A-Za-z0-9_]*):\s*(.*)$/.exec(line);
    if (!m) continue;
    const key = m[1];
    let raw = m[2].trim();
    if (raw === '' || raw === 'null' || raw === '~') {
      data[key] = null;
    } else if (raw.startsWith('[') && raw.endsWith(']')) {
      data[key] = raw
        .slice(1, -1)
        .split(',')
        .map((s) => s.trim().replace(/^['"]|['"]$/g, ''))
        .filter(Boolean);
    } else if (/^-?\d+$/.test(raw)) {
      data[key] = Number(raw);
    } else {
      data[key] = raw.replace(/^['"]|['"]$/g, '');
    }
  }
  return { data, bodyStart: i + 1 };
}

// ---------------------------------------------------------------- git helpers

export function commitExists(root, sha) {
  try {
    execFileSync('git', ['cat-file', '-e', `${sha}^{commit}`], {
      cwd: root,
      stdio: ['ignore', 'ignore', 'ignore'],
    });
    return true;
  } catch {
    return false;
  }
}

/** commit 是否被 HEAD 祖先链、tag 或 branch 中的任意稳定引用覆盖。 */
export function commitReachable(root, sha) {
  try {
    execFileSync('git', ['merge-base', '--is-ancestor', sha, 'HEAD'], {
      cwd: root,
      stdio: ['ignore', 'ignore', 'ignore'],
    });
    return true;
  } catch {
    /* 继续尝试其它引用 */
  }
  for (const args of [
    ['tag', '--contains', sha],
    ['branch', '-a', '--contains', sha],
  ]) {
    try {
      const out = execFileSync('git', args, { cwd: root, encoding: 'utf8' });
      if (out.trim()) return true;
    } catch {
      /* 继续 */
    }
  }
  return false;
}

// ---------------------------------------------------------------- discovery

export function discoverFeatureDirs(root) {
  const specsRoot = path.resolve(root, 'specs');
  const out = [];
  if (!fs.existsSync(specsRoot)) return out;
  for (const domain of fs.readdirSync(specsRoot, { withFileTypes: true })) {
    if (!domain.isDirectory() || domain.name.startsWith('_')) continue;
    if (!DOMAINS.includes(domain.name)) continue;
    const domainDir = path.join(specsRoot, domain.name);
    for (const feature of fs.readdirSync(domainDir, { withFileTypes: true })) {
      if (!feature.isDirectory()) continue;
      out.push({
        domain: domain.name,
        name: feature.name,
        dir: path.posix.join('specs', domain.name, feature.name),
      });
    }
  }
  return out.sort((a, b) => a.dir.localeCompare(b.dir));
}

// ---------------------------------------------------------------- lint

export function lintRepo(root) {
  const violations = [];
  const add = (check, file, line, message) => violations.push({ check, file, line, message });
  const stats = { features: 0, markdown: 0, linksChecked: 0, manifests: 0 };

  const mdFiles = collectMarkdown(root);
  stats.markdown = mdFiles.length;

  const features = discoverFeatureDirs(root);
  stats.features = features.length;

  const canonicalDefs = new Map(); // id -> "file:line"
  const featureDefs = []; // { feature, id, file, line }
  const allLegacyIds = new Map(); // legacy id -> feature dir
  const featureIds = new Map(); // canonical feature id -> feature dir

  // ---- C10 + C1 + C2 + C9 -------------------------------------------
  for (const feature of features) {
    for (const doc of FEATURE_DOCS) {
      const rel = path.posix.join(feature.dir, doc);
      if (!exists(root, rel)) {
        add('C10-feature-docs', rel, 0, `feature 目录缺少必需文档 ${doc}`);
      }
    }
    const specRel = path.posix.join(feature.dir, 'spec.md');
    if (!exists(root, specRel)) continue;
    const specText = read(root, specRel);
    const specLines = specText.split(/\r?\n/);
    const { data } = parseFrontMatter(specText);

    if (!data) {
      add('C1-metadata', specRel, 1, '缺少 YAML front matter（machine-readable metadata）');
    } else {
      for (const key of REQUIRED_METADATA) {
        if (!(key in data)) add('C1-metadata', specRel, 1, `metadata 缺少必填键 ${key}`);
      }
      const enums = {
        spec_status: SPEC_STATUSES,
        approval: APPROVAL_STATUSES,
        implementation_status: IMPLEMENTATION_STATUSES,
        validation_status: VALIDATION_STATUSES,
        release_status: RELEASE_STATUSES,
        domain: DOMAINS,
      };
      for (const [key, allowed] of Object.entries(enums)) {
        if (key in data && data[key] !== null && !allowed.includes(data[key])) {
          add('C1-status', specRel, 1, `非法状态 ${key}=${data[key]}（允许：${allowed.join('|')}）`);
        }
      }
      if (data.domain && data.domain !== feature.domain) {
        add('C1-metadata', specRel, 1, `metadata domain=${data.domain} 与目录 ${feature.domain} 不一致`);
      }
      if (data.canonical_spec && data.canonical_spec !== specRel) {
        add('C1-metadata', specRel, 1, `canonical_spec=${data.canonical_spec} 与自身路径不一致`);
      }
      if (data.spec_status === 'Approved') {
        if (data.approval !== 'Approved') {
          add('C1-approval', specRel, 1, 'spec_status=Approved 但 approval≠Approved');
        }
        for (const key of ['approver', 'approval_date', 'approval_evidence']) {
          if (!data[key]) {
            add('C1-approval', specRel, 1, `spec_status=Approved 但缺少真实批准证据字段 ${key}`);
          }
        }
      }
      if (data.approval === 'Approved' && data.spec_status !== 'Approved') {
        add('C1-approval', specRel, 1, 'approval=Approved 但 spec_status≠Approved');
      }
      if (['Accepted', 'Released'].includes(data.release_status) && data.validation_status !== 'PASS') {
        add('C1-approval', specRel, 1, `release_status=${data.release_status} 但 validation_status≠PASS`);
      }
      const n = [data.required_fields, data.additional_fields, data.total_fields];
      if (n.every((v) => typeof v === 'number')) {
        if (data.required_fields + data.additional_fields !== data.total_fields) {
          add(
            'C7-numeric',
            specRel,
            1,
            `数字不一致：required_fields(${data.required_fields}) + additional_fields(${data.additional_fields}) != total_fields(${data.total_fields})`,
          );
        }
      }
      if (Array.isArray(data.legacy_ids) === false && data.legacy_ids !== null) {
        add('C1-metadata', specRel, 1, 'legacy_ids 必须是列表');
      }
    }

    // C9: gate/step 术语必须在规格中显式定义
    for (const term of ['Workflow Step', 'Validation Gate', 'Recording Step']) {
      if (!specText.includes(term)) {
        add('C9-terminology', specRel, 0, `规格未定义工作流概念 "${term}"`);
      }
    }

    // C2: SPEC 不得依赖 DESIGN 定义 requirement
    const allowRe = /sdd-lint:allow-design-ref|非规范引用|informative/;
    specLines.forEach((line, idx) => {
      if (!line.includes('design.md')) return;
      const normative = /(REQ-\d{3}|AC-\d{3}|SPEC-\d+\.\d+|要求|验收)/.test(line);
      if (normative && !allowRe.test(line)) {
        add(
          'C2-reverse-dependency',
          specRel,
          idx + 1,
          'SPEC 条款引用 design.md 作为规范定义来源（反向依赖）',
        );
      }
    });

    // C3: canonical ID 定义（按文件类型限定可定义的 ID 种类，避免把"引用表"误判为定义）
    //   spec.md      -> REQ / AC
    //   tasks.md     -> TASK
    //   test-plan.md -> TEST
    // 首列必须恰好是一个 canonical ID（多 ID 的映射行视为引用）。
    const DEF_KINDS = { 'spec.md': ['REQ', 'AC'], 'tasks.md': ['TASK'], 'test-plan.md': ['TEST'] };
    for (const doc of FEATURE_DOCS) {
      const kinds = DEF_KINDS[doc] || [];
      if (kinds.length === 0) continue;
      const docRel = path.posix.join(feature.dir, doc);
      if (!exists(root, docRel)) continue;
      read(root, docRel)
        .split(/\r?\n/)
        .forEach((line, idx) => {
          const cells = line.trim().split('|');
          if (cells.length < 3) return;
          const first = cells[1].trim().replace(/`/g, '');
          const m = /^([A-Z][A-Z0-9-]*(REQ|AC|TASK|TEST)-\d{3})$/.exec(first);
          if (!m || !kinds.includes(m[2])) return;
          const id = m[1];
          const where = `${docRel}:${idx + 1}`;
          const prev = canonicalDefs.get(id);
          if (prev) {
            // 同一文件内的重复出现视为"映射/引用表"，只有跨文件重复才是真正的冲突定义
            if (!prev.startsWith(`${docRel}:`)) {
              add('C3-duplicate-id', docRel, idx + 1, `canonical ID ${id} 重复定义（另见 ${prev}）`);
            }
            return;
          }
          canonicalDefs.set(id, where);
          featureDefs.push({ feature, id, file: docRel, line: idx + 1 });
        });
    }

    // 记录 legacy ID，供迁移映射检查
    if (Array.isArray(data?.legacy_ids)) {
      for (const legacy of data.legacy_ids) allLegacyIds.set(legacy, feature.dir);
    }
    if (data?.id) featureIds.set(data.id, feature.dir);

    // C12: DESIGN 不得重复定义 ADR
    const designRel = path.posix.join(feature.dir, 'design.md');
    if (exists(root, designRel)) {
      const designText = read(root, designRel);
      if (/^\|\s*ADR-\d{4}\s*\|\s*\d{4}-\d{2}-\d{2}\s*\|/m.test(designText)) {
        add('C12-adr-duplication', designRel, 0, 'design.md 重复定义 ADR（ADR 唯一定义在 docs/adr/）');
      }
    }
  }

  // ---- C3b: 引用完整性 + legacy 迁移映射 ---------------------------------
  const migrationRel = 'docs/migration/SDD_ARCHITECTURE_MIGRATION.md';
  const migrationText = exists(root, migrationRel) ? read(root, migrationRel) : '';
  if (!migrationText) {
    add('C3b-migration-map', migrationRel, 0, '缺少 SDD 迁移说明（旧 ID → canonical ID 映射）');
  }
  for (const [legacy, featureDir] of allLegacyIds) {
    if (migrationText && !migrationText.includes(legacy)) {
      add('C3b-migration-map', featureDir, 0, `legacy ID ${legacy} 未出现在迁移映射 ${migrationRel}`);
    }
  }
  const namespaces = [...featureIds.keys()].sort((a, b) => b.length - a.length);
  const referenced = new Map();
  for (const rel of mdFiles) {
    read(root, rel)
      .split(/\r?\n/)
      .forEach((line, idx) => {
        CANONICAL_ID_RE.lastIndex = 0;
        let m;
        while ((m = CANONICAL_ID_RE.exec(line)) !== null) {
          const id = m[1];
          if (!namespaces.some((ns) => id.startsWith(`${ns}-`))) continue;
          if (!referenced.has(id)) referenced.set(id, `${rel}:${idx + 1}`);
        }
      });
  }
  for (const [id, where] of referenced) {
    if (!canonicalDefs.has(id)) {
      add('C3b-reference', where.split(':')[0], 0, `引用了未定义的 canonical ID ${id}`);
    }
  }

  // ---- C12: 根 DESIGN.md 也不得重复定义 ADR --------------------------------
  if (exists(root, 'DESIGN.md')) {
    if (/^\|\s*ADR-\d{4}\s*\|\s*\d{4}-\d{2}-\d{2}\s*\|/m.test(read(root, 'DESIGN.md'))) {
      add('C12-adr-duplication', 'DESIGN.md', 0, 'DESIGN.md 重复定义 ADR（ADR 唯一定义在 docs/adr/）');
    }
  }

  // ---- C8: 模糊生命周期状态词 ---------------------------------------------
  for (const rel of ['README.md', 'SPEC.md']) {
    if (!exists(root, rel)) continue;
    read(root, rel)
      .split(/\r?\n/)
      .forEach((line, idx) => {
        if (/(已交付|Delivered)/.test(line)) {
          add('C8-ambiguous-status', rel, idx + 1, '使用模糊的单一交付状态词（已交付/Delivered）；应使用四维状态');
        }
      });
  }

  // ---- C4: 相对链接 --------------------------------------------------------
  const linkResult = checkLinks(root, mdFiles);
  stats.linksChecked = linkResult.checked;
  for (const rel of linkResult.missingInputs) {
    add('C4-link', rel, 0, 'Markdown 输入文件缺失');
  }
  for (const { file, target } of linkResult.broken) {
    add('C4-link', file, 0, `相对链接失效 -> ${target}`);
  }

  // ---- C5: 验证脚本声明的必需路径必须存在 ---------------------------------
  const toolFiles = [];
  const walk = (dir) => {
    const abs = path.resolve(root, dir);
    if (!fs.existsSync(abs)) return;
    for (const e of fs.readdirSync(abs, { withFileTypes: true })) {
      if (e.isDirectory()) walk(path.posix.join(dir, e.name));
      else toolFiles.push(path.posix.join(dir, e.name));
    }
  };
  walk('tools');
  for (const rel of toolFiles) {
    if (!rel.endsWith('.sh')) continue;
    read(root, rel)
      .split(/\r?\n/)
      .forEach((line, idx) => {
        const m = /^\s*sdd_require_path\s+("([^"]+)"|'([^']+)'|(\S+))\s*$/.exec(line);
        if (!m) return;
        const target = m[2] || m[3] || m[4];
        if (target.includes('$')) return; // 动态路径由运行期判定
        if (!exists(root, target)) {
          add('C5-validator-path', rel, idx + 1, `sdd_require_path 引用的路径不存在: ${target}`);
        }
      });
  }

  // ---- C6: Validation commit 可达性 ---------------------------------------
  const manifests = [];
  const walkJson = (dir) => {
    const abs = path.resolve(root, dir);
    if (!fs.existsSync(abs)) return;
    for (const e of fs.readdirSync(abs, { withFileTypes: true })) {
      if (e.isDirectory()) walkJson(path.posix.join(dir, e.name));
      else if (e.name === 'manifest.json') manifests.push(path.posix.join(dir, e.name));
    }
  };
  walkJson('evidence');
  stats.manifests = manifests.length;
  for (const rel of manifests) {
    let data;
    try {
      data = JSON.parse(read(root, rel));
    } catch (err) {
      add('C6-provenance', rel, 0, `manifest.json 无法解析：${err.message}`);
      continue;
    }
    for (const key of ['implementation_commit', 'validator_commit', 'validation_commit']) {
      const sha = data[key];
      if (!sha || typeof sha !== 'string') continue;
      if (!/^[0-9a-f]{7,40}$/i.test(sha)) continue;
      if (!commitExists(root, sha)) {
        add('C6-provenance', rel, 0, `${key}=${sha} 在对象库中不存在`);
      } else if (!commitReachable(root, sha)) {
        add('C6-provenance', rel, 0, `${key}=${sha} 不可达（无稳定 tag / branch / HEAD 祖先引用）`);
      }
    }
    if (data.validation_ref) {
      try {
        execFileSync('git', ['rev-parse', '--verify', '--quiet', data.validation_ref], {
          cwd: root,
          stdio: ['ignore', 'ignore', 'ignore'],
        });
      } catch {
        add('C6-provenance', rel, 0, `validation_ref 不存在: ${data.validation_ref}`);
      }
    }
  }

  return { violations, stats, features };
}
