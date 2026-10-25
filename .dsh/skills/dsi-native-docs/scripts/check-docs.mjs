#!/usr/bin/env node
/**
 * check-docs.mjs — 校验「目录名.md」原生文档机制。
 *
 * 用法:
 *   node check-docs.mjs <dir> [<dir>...]
 *
 * 规则:
 *   - 递归扫描显式传入的目录；跳过隐藏目录与 build/node_modules/.xmake 等。
 *   - 含源码文件的目录必须有 <目录名>.md；已存在的同名文档一律校验。
 *   - 检查: BOM、H1 含目录名、两行 blockquote(摘要/联系)、五节顺序、
 *           相对链接可解析、索引覆盖目录内全部非 md 文件、子目录被提及。
 *
 * 退出码: 0 全部通过; 1 有错误; 2 用法错误。
 */
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { basename, dirname, extname, join, resolve, sep } from "node:path";

const SKIP_DIRS = new Set([
  "build", "out", "dist", "target", "node_modules", ".xmake", ".git",
  ".vs", ".idea", ".vscode", "__pycache__", "obj", "bin",
]);
const SOURCE_EXT = new Set([
  ".h", ".hpp", ".hh", ".hxx", ".h++", ".inl", ".ipp",
  ".c", ".cc", ".cpp", ".cxx", ".c++",
  ".lua", ".py", ".js", ".mjs", ".cjs", ".ts", ".tsx", ".jsx",
  ".rs", ".go", ".java", ".kt", ".cs", ".sh", ".bash", ".ps1",
  ".rb", ".php", ".swift", ".m", ".mm",
]);
const SECTIONS = ["truth", "约束", "索引", "决策", "教训"];

const errors = [];
const warnings = [];
let dirsScanned = 0;
let docsValidated = 0;
let requiredMissing = 0;

const roots = process.argv.slice(2).map((p) => resolve(p));
if (roots.length === 0) {
  console.error("用法: node check-docs.mjs <dir> [<dir>...]");
  process.exit(2);
}
for (const r of roots) {
  if (!existsSync(r) || !statSync(r).isDirectory()) {
    console.error(`不是目录: ${r}`);
    process.exit(2);
  }
}

const show = (p) => {
  const rel = p.startsWith(process.cwd() + sep) ? p.slice(process.cwd().length + 1) : p;
  return rel.split(sep).join("/");
};

function scan(root) {
  const dirs = new Map(); // dir -> { files, subdirs }
  (function walk(dir) {
    const files = [];
    const subdirs = [];
    for (const e of readdirSync(dir, { withFileTypes: true })) {
      if (e.name.startsWith(".")) continue;
      if (e.isDirectory()) {
        if (SKIP_DIRS.has(e.name)) continue;
        subdirs.push(e.name);
        walk(join(dir, e.name));
      } else if (e.isFile()) {
        files.push(e.name);
      }
    }
    dirs.set(dir, { files, subdirs });
  })(root);
  return dirs;
}

function checkDoc(docPath, dirName, dir, files, subdirs, dirs) {
  const text = readFileSync(docPath, "utf8");
  let ok = true;
  const bad = (msg) => { ok = false; errors.push(`FAIL ${show(docPath)}: ${msg}`); };

  if (text.charCodeAt(0) === 0xfeff) bad("文件含 UTF-8 BOM");

  const lines = text.split(/\r?\n/);
  if (text.includes("\r\n")) warnings.push(`WARN ${show(docPath)}: 使用 CRLF 换行（建议 LF）`);

  const firstNonEmpty = lines.findIndex((l) => l.trim() !== "");
  if (firstNonEmpty < 0) { bad("空文档"); return false; }
  const h1 = lines[firstNonEmpty].trim();
  const m = /^#\s+(.+)$/.exec(h1);
  if (!m) bad(`首个非空行不是 H1: ${h1}`);
  else if (!m[1].startsWith(dirName)) bad(`H1 应以目录名 "${dirName}" 开头: ${h1}`);

  const head = lines.slice(firstNonEmpty + 1, firstNonEmpty + 12);
  const iAbs = head.findIndex((l) => /^>\s*\*\*摘要\*\*/.test(l.trim()));
  const iCon = head.findIndex((l) => /^>\s*\*\*联系\*\*/.test(l.trim()));
  if (iAbs < 0) bad("H1 下缺少 '> **摘要**：' 行");
  if (iCon < 0) bad("H1 下缺少 '> **联系**：' 行");
  if (iAbs >= 0 && iCon >= 0 && iAbs > iCon) bad("摘要 行应在 联系 行之前");

  const h2 = [...text.matchAll(/^## (.+)$/gm)].map((x) => x[1].trim());
  const same = h2.length === SECTIONS.length && h2.every((s, i) => s === SECTIONS[i]);
  if (!same) bad(`五节标题应为 [${SECTIONS.join(" / ")}]，实际 [${h2.join(" / ")}]`);

  for (const mm of text.matchAll(/\[[^\]]*\]\(([^)\s]+)(?:\s+"[^"]*")?\)/g)) {
    let target = mm[1];
    if (/^(https?:|mailto:|tel:|#)/i.test(target)) continue;
    if (target.startsWith("//")) continue;
    target = target.replace(/[#?].*$/, "");
    if (!target) continue;
    let decoded = target;
    try { decoded = decodeURIComponent(target); } catch { /* keep raw */ }
    const abs = resolve(dirname(docPath), decoded);
    if (!existsSync(abs)) bad(`相对链接无法解析: ${mm[1]}`);
  }

  const listed = files.filter((f) => !f.endsWith(".md"));
  for (const f of listed) {
    if (!text.includes(f)) bad(`索引未覆盖文件: ${f}`);
  }
  for (const d of subdirs) {
    const dp = join(dir, d);
    const meta = dirs.get(dp);
    if (!meta) continue;
    const childHasSource = meta.files.some((f) => SOURCE_EXT.has(extname(f).toLowerCase()));
    const childDoc = join(dp, `${d}.md`);
    if (childHasSource || existsSync(childDoc)) {
      if (!text.includes(d)) bad(`正文未提及子目录: ${d}/`);
    }
  }
  for (const f of files) {
    if (f.endsWith(".md") && f !== `${dirName}.md`) {
      warnings.push(`WARN ${show(docPath)}: 目录内存在额外 Markdown "${f}"（一个目录只应有一篇官方文档）`);
    }
  }
  if (ok) console.log(`OK   ${show(docPath)}`);
  return ok;
}

for (const root of roots) {
  const dirs = scan(root);

  // 反向遍历（前序插入的逆序）保证先算子树。
  const subtreeHasSource = new Map();
  const keys = [...dirs.keys()].reverse();
  for (const dir of keys) {
    const meta = dirs.get(dir);
    const direct = meta.files.some((f) => SOURCE_EXT.has(extname(f).toLowerCase()));
    const childHas = meta.subdirs.some((d) => subtreeHasSource.get(join(dir, d)) === true);
    subtreeHasSource.set(dir, direct || childHas);
  }

  for (const [dir, meta] of dirs) {
    dirsScanned++;
    const name = basename(dir);
    const docPath = join(dir, `${name}.md`);
    const hasDoc = existsSync(docPath);
    const directSource = meta.files.some((f) => SOURCE_EXT.has(extname(f).toLowerCase()));
    const required = directSource || (dir !== root && subtreeHasSource.get(dir) === true);

    if (required && !hasDoc) {
      requiredMissing++;
      errors.push(`FAIL ${show(join(dir, name + ".md"))}: 缺少同名文档（该目录含源码或含源码子目录）`);
    }
    if (hasDoc) {
      docsValidated++;
      checkDoc(docPath, name, dir, meta.files, meta.subdirs, dirs);
    }
  }
}

console.log("");
console.log(`扫描目录: ${dirsScanned}  校验文档: ${docsValidated}  缺失文档: ${requiredMissing}`);
if (warnings.length) {
  console.log(`警告 ${warnings.length} 条:`);
  for (const w of warnings) console.log("  " + w);
}
if (errors.length) {
  console.log(`错误 ${errors.length} 条:`);
  for (const e of errors) console.log("  " + e);
  process.exit(1);
}
console.log("全部检查通过。");
