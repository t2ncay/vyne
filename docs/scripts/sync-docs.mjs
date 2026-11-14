#!/usr/bin/env node
// ---------------------------------------------------------------------------
// sync-docs.mjs — regenerate content/*.html + nav.json from docs/src/*.
//
//   node scripts/sync-docs.mjs            # sync sources, then build
//   node scripts/sync-docs.mjs --no-build # only sync
//
// Sources carry a tiny front-matter block:
//
//   ---
//   id: regions
//   title: Memory & Regions
//   group: Language Reference
//   icon: fa-layer-group
//   order: 9
//   description: A one-line lede.
//   ---
//
// .md sources are rendered with the site Markdown pipeline. .html sources are
// inserted verbatim (used by the introduction hero and image-rich showcase).
// Library reference pages are generated from the in-repo manuals under
// vyne/modules/external/<lib>/README.md so they never drift from the code.
// ---------------------------------------------------------------------------

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { parseFrontMatter, extractTitle, renderMarkdown } from "./lib/markdown.mjs";
import { stripHtml, escapeHtml } from "./lib/slug.mjs";
import { rewriteRepositoryLinks } from "./lib/repository-links.mjs";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const DOCS = path.resolve(__dirname, "..");
const ROOT = path.resolve(DOCS, "..");
const SRC = path.join(DOCS, "src");
const argv = process.argv.slice(2);
const NO_BUILD = argv.includes("--no-build");
const outputIndex = argv.indexOf("--output-dir");
if (outputIndex !== -1 && (!argv[outputIndex + 1] || argv[outputIndex + 1].startsWith("--"))) {
  throw new Error("--output-dir requires a directory");
}
const OUTPUT = outputIndex === -1 ? DOCS : path.resolve(argv[outputIndex + 1]);
const CONTENT = path.join(OUTPUT, "content");
const NAV_FILE = path.join(OUTPUT, "nav.json");

const GROUPS = [
  ["Getting Started", "fa-rocket"],
  ["Language Reference", "fa-book"],
  ["Standard Library", "fa-cube"],
  ["Numerical Libraries", "fa-square-root-variable"],
  ["DSP & Control", "fa-wave-square"],
  ["Data & Plotting", "fa-chart-line"],
  ["Image & Vision", "fa-image"],
  ["Graphics & Audio", "fa-gamepad"],
  ["Embedded Buses", "fa-plug"],
  ["Domain Libraries", "fa-dna"],
  ["Helpers", "fa-toolbox"],
  ["Showcase", "fa-trophy"],
  ["Reference", "fa-key"],
];

// External libraries whose manuals live under vyne/modules/external/<id>/README.md.
// `id` is also the URL slug and the module name.
const LIBRARIES = [
  {
    id: "vlin",
    title: "vlin",
    group: "Numerical Libraries",
    icon: "fa-table-cells-large",
    order: 10,
    lede: "Dense matrices and vectors: construction, arithmetic, products, reductions, and in-place training kernels.",
  },
  {
    id: "vml",
    title: "vml",
    group: "Numerical Libraries",
    icon: "fa-network-wired",
    order: 20,
    lede: "CPU dense/MLP primitives over vlin matrices: Dense layers, Sequential, activations, losses, and optimizer helpers.",
  },
  {
    id: "vfft",
    title: "vfft",
    group: "Numerical Libraries",
    icon: "fa-wave-square",
    order: 30,
    lede: "Iterative radix-2 FFT with cached twiddle factors, inverse transforms, and magnitude/phase helpers.",
  },
  {
    id: "vrand",
    title: "vrand",
    group: "Numerical Libraries",
    icon: "fa-dice",
    order: 40,
    lede: "Continuous and discrete distributions, sampling utilities, and explicit RNG state access.",
  },
  {
    id: "vstat",
    title: "vstat",
    group: "Numerical Libraries",
    icon: "fa-chart-simple",
    order: 50,
    lede: "Descriptive statistics, correlation, regression, histograms, streaming moments, and image-quality metrics.",
  },
  {
    id: "vcomplex",
    title: "vcomplex",
    group: "Numerical Libraries",
    icon: "fa-infinity",
    order: 60,
    lede: "Scalar complex arithmetic, elementary functions, polar helpers, and split-array element-wise kernels.",
  },
  {
    id: "vinterp",
    title: "vinterp",
    group: "Numerical Libraries",
    icon: "fa-bezier-curve",
    order: 70,
    lede: "1-D interpolation over sorted curves and 2-D interpolation over row-major grids.",
  },
  {
    id: "voptim",
    title: "voptim",
    group: "Numerical Libraries",
    icon: "fa-arrow-trend-down",
    order: 80,
    lede: "In-place optimizers (SGD, momentum, Adam, AdamW, RMSProp, Adagrad) and learning-rate schedules.",
  },
  {
    id: "vsparse",
    title: "vsparse",
    group: "Numerical Libraries",
    icon: "fa-braille",
    order: 90,
    lede: "Compressed sparse row matrices, SpMV, residuals, and conjugate gradient.",
  },
  {
    id: "vdecomp",
    title: "vdecomp",
    group: "Numerical Libraries",
    icon: "fa-divide",
    order: 100,
    lede: "LU, Householder QR, Cholesky, symmetric Jacobi eigen, and singular values for dense matrices.",
  },
  {
    id: "vtensor",
    title: "vtensor",
    group: "Numerical Libraries",
    icon: "fa-cubes-stacked",
    order: 110,
    lede: "Region-local device tensor handles with HWC shape metadata and packed-buffer crop staging.",
  },
  {
    id: "vnn",
    title: "vnn",
    group: "Numerical Libraries",
    icon: "fa-network-wired",
    order: 120,
    lede: "Explicit device-resident convolution, ReLU, pixel shuffle, MSE gradients, and AdamW training blocks.",
  },
  {
    id: "vraylib",
    title: "vraylib Native Reference",
    group: "Graphics & Audio",
    icon: "fa-gamepad",
    order: 20,
    lede: "Compiled desktop raylib 6.2, raymath, and rlgl with checked resources and a managed external facade.",
  },
  {
    id: "vsignal",
    title: "vsignal",
    group: "DSP & Control",
    icon: "fa-signal",
    order: 10,
    lede: "Streaming and offline DSP: windows, moving averages, IIR/FIR filters, resampling, and spectra.",
  },
  {
    id: "vcontrol",
    title: "vcontrol",
    group: "DSP & Control",
    icon: "fa-sliders",
    order: 20,
    lede: "PID, complementary filters, scalar and constant-velocity Kalman filters, and state-space models.",
  },
  {
    id: "vcsv",
    title: "vcsv",
    group: "Data & Plotting",
    icon: "fa-file-csv",
    order: 10,
    lede: "RFC 4180 CSV/TSV parsing and writing with headers, quoting, typed columns, and vfs-backed I/O.",
  },
  {
    id: "vjson",
    title: "vjson",
    group: "Data & Plotting",
    icon: "fa-code",
    order: 20,
    lede: "JSON parsing, serialization, validation, and path access.",
  },
  {
    id: "vtermplot",
    title: "vtermplot",
    group: "Data & Plotting",
    icon: "fa-chart-area",
    order: 30,
    lede: "Terminal line, scatter, bar, histogram, heatmap, and sparkline plots with UTF-8 or ASCII output.",
  },
  {
    id: "vimage",
    title: "vimage",
    group: "Image & Vision",
    icon: "fa-image",
    order: 10,
    lede: "PNG/JPEG/BMP/TGA decode and encode, bilinear resize, crop, and grayscale on HWC Float64 pixels.",
  },
  {
    id: "vconv",
    title: "vconv",
    group: "Image & Vision",
    icon: "fa-border-all",
    order: 20,
    lede: "CPU tensor convolution and explicit device forward/backward primitives, activations, pixel shuffle, and weight bundles.",
  },
  {
    id: "vi2c",
    title: "vi2c",
    group: "Embedded Buses",
    icon: "fa-circle-nodes",
    order: 10,
    lede: "Typed, range-checked I2C master helpers for the Arduino Uno target.",
  },
  {
    id: "vspi",
    title: "vspi",
    group: "Embedded Buses",
    icon: "fa-diagram-project",
    order: 20,
    lede: "Typed, range-checked SPI master helpers for the Arduino Uno target.",
  },
  {
    id: "vbio",
    title: "vbio",
    group: "Domain Libraries",
    icon: "fa-dna",
    order: 10,
    lede: "DNA, RNA, and protein sequences, the genetic code, translation, and nucleotide statistics.",
  },
];

function die(message) {
  console.error(`\n✖ ${message}\n`);
  process.exit(1);
}

function walk(dir) {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const abs = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...walk(abs));
    else if (/\.(md|html)$/i.test(entry.name)) out.push(abs);
  }
  return out;
}

function firstParagraph(markdown) {
  const body = markdown
    .replace(/^#{1,6}\s+.*$/gm, "")
    .replace(/```[\s\S]*?```/g, "")
    .trim();
  const paragraph = body.split(/\r?\n\s*\r?\n/).find((p) => p.trim() && !p.trim().startsWith("|"));
  return paragraph ? stripHtml(paragraph).replace(/\s+/g, " ").trim() : "";
}

function header({ title, group, description }) {
  return (
    `<div class="page-eyebrow">${escapeHtml(group)}</div>\n` +
    `<h1>${escapeHtml(title)}</h1>\n` +
    (description ? `<p class="lede">${escapeHtml(description)}</p>\n` : "")
  );
}

function arduinoApiSection() {
  const readme = path.join(ROOT, "vyne", "runtime", "arduino", "README.md");
  if (!fs.existsSync(readme)) return "";
  const text = fs.readFileSync(readme, "utf8").replace(/\r\n/g, "\n");
  const start = text.indexOf("## API reference");
  const end = text.indexOf("## Common patterns");
  if (start === -1) return "";
  return text.slice(start, end === -1 ? undefined : end).trim()
    .replace(/\(#what-is-not-bound-and-why\)/g, "(#what-is-not-bound)")
    .replace(/\(#memory-model\)/g, "(#the-memory-model-on-avr)");
}

function expandPlaceholders(markdown) {
  if (markdown.includes("<!-- ARDUINO_API -->")) {
    markdown = markdown.replace("<!-- ARDUINO_API -->", arduinoApiSection());
  }
  if (markdown.includes("<!-- VRAYLIB_MANAGED -->")) {
    const manual = path.join(ROOT, "vyne/modules/external/vraylib/MANAGED.md");
    const { body } = extractTitle(fs.readFileSync(manual, "utf8"));
    markdown = markdown.replace("<!-- VRAYLIB_MANAGED -->", rewriteRepositoryLinks(body, manual, ROOT, siteManuals()));
  }
  return markdown;
}

function siteManuals() {
  const entries = LIBRARIES.map((lib) => [path.join(ROOT, "vyne/modules/external", lib.id, "README.md"), lib.id]);
  entries.push([path.join(ROOT, "vyne/modules/external/vraylib/MANAGED.md"), "vraylib-managed"]);
  for (const file of walk(SRC)) {
    const { data } = parseFrontMatter(fs.readFileSync(file, "utf8"));
    entries.push([file, data.id || path.basename(file, path.extname(file))]);
  }
  return new Map(entries);
}

function libraryManual(lib) {
  if (lib.manual === false) return null;
  const manual = path.join(ROOT, "vyne", "modules", "external", lib.id, "README.md");
  if (!fs.existsSync(manual)) return null;

  let text = fs.readFileSync(manual, "utf8");
  const { body } = parseFrontMatter(text);
  const { body: withoutH1 } = extractTitle(body);

  // The manuals predate the native/external import split in a few places.
  let normalized = withoutH1
    .replace(/^(\s*)\*\*(Version|Status|Module name|Import path|Requires):\*\*.*$/gm, "")
    .replace(/use lib "/g, 'use external "');

  if (lib.id === "vbio") {
    normalized = normalized
      .replace(/use external "vbio\/bio\.vy";/g, 'use external "vbio/vbio.vy";')
      .replace(/`bio\.vy`/g, "`vbio.vy`")
      .replace(/\bmodule bio;/g, "module vbio;")
      .replace(/\bbio\.(?=[A-Za-z_])/g, "vbio.")
      .replace(/\bvlinalg\b/g, "vlin");
  }

  if (lib.id === "vlin") {
    normalized = normalized.replace(/the `use lib` path/g, "the `use external` path");
  }

  normalized = normalized.replace(/\n{3,}/g, "\n\n").trim();
  normalized = rewriteRepositoryLinks(normalized, manual, ROOT, siteManuals());
  return renderMarkdown(normalized);
}

function manualImport(lib) {
  return lib.id === "vbio" ? `use external "vbio/vbio.vy";` : `use external "${lib.id}/${lib.id}.vy";`;
}

function manualDependencies(lib) {
  const map = {
    vlin: "`vmath`, `vcolors`",
    vml: "`vlin`, `vmath`",
    vfft: "`vmath`",
    vrand: "`vmath`",
    vstat: "`vmath`",
    vcomplex: "`vmath`",
    vinterp: "`vstat` (series/curve types)",
    voptim: "none beyond the core runtime",
    vsparse: "`vmath`",
    vdecomp: "`vmath`",
    vtensor: "an active `@device` region for device helpers",
    vnn: "`vtensor` and an active `@device` region",
    vraylib: "GCC, CMake, make and platform graphics libraries (compiled desktop)",
    vsignal: "`vfft`, `vstat` (optional spectral/statistics helpers)",
    vcontrol: "`vsignal` (recommended for sensor pre-filtering)",
    vcsv: "`vfs`, `vcore`",
    vjson: "`vcore`",
    vtermplot: "`vcore` (string builder)",
    vimage: "none (stb codecs are compiled in)",
    vconv: "`vimage` for the SR pipeline",
    vi2c: "`arduino` (AVR target)",
    vspi: "`arduino` (AVR target)",
    vbio: "none beyond the core runtime",
    vchem: "none beyond the core runtime",
  };
  return map[lib.id] || "none";
}

function libraryPage(lib) {
  const manual = libraryManual(lib);
  const facts =
    `<div class="table-wrap"><table><thead><tr><th>Quick fact</th><th>Value</th></tr></thead><tbody>` +
    `<tr><td>Module</td><td><code class="inline">${escapeHtml(lib.id)}</code></td></tr>` +
    `<tr><td>Import</td><td><code class="inline">${escapeHtml(manualImport(lib))}</code></td></tr>` +
    `<tr><td>Depends on</td><td>${manualDependencies(lib).replace(/`([^`]+)`/g, '<code class="inline">$1</code>')}</td></tr>` +
    `<tr><td>Status</td><td>Unstable — the public surface may change between releases.</td></tr>` +
    `</tbody></table></div>\n`;

  const notice =
    `<div class="admonition adm-note"><i class="fas fa-circle-info"></i><div class="adm-body">` +
    `<div class="adm-title">Generated reference</div>` +
    `<p>This page is generated from the library's in-repo manual (` +
    `<code class="inline">vyne/modules/external/${escapeHtml(lib.id)}/README.md</code>) ` +
    `so signatures and behaviour stay aligned with the source tree. Examples assume a desktop target unless the text says otherwise.</p>` +
    `</div></div>\n`;

  const body = manual || `<p>${escapeHtml(lib.lede || "")}</p>`;
  return header({ title: lib.title, group: lib.group, description: lib.lede }) + facts + notice + body + "\n";
}

// --- collect source pages ---------------------------------------------------
if (!fs.existsSync(SRC)) die(`Source directory not found: ${SRC}`);

const pages = [];
for (const file of walk(SRC)) {
  const raw = fs.readFileSync(file, "utf8");
  const { data: fm, body } = parseFrontMatter(raw);
  const isHtml = /\.html$/i.test(file);
  const { title: h1, body: withoutH1 } = isHtml ? { title: "", body } : extractTitle(body);
  const id = fm.id || path.basename(file, path.extname(file));
  const title = fm.title || h1 || id;
  const group = fm.group || "Docs";
  const icon = fm.icon || "fa-file-lines";
  const order = fm.order ? Number(fm.order) : 999;
  const description = fm.description || firstParagraph(withoutH1);
  const showHeader = fm.header !== "false";
  const content = (showHeader ? header({ title, group, description }) : "") +
    (isHtml ? withoutH1.trim() : renderMarkdown(expandPlaceholders(withoutH1)));
  pages.push({ id, title, group, icon, order, description, content, source: path.relative(DOCS, file) });
}

// --- library pages ----------------------------------------------------------
for (const lib of LIBRARIES) {
  pages.push({
    id: lib.id,
    title: lib.title,
    group: lib.group,
    icon: lib.icon,
    order: lib.order,
    description: lib.lede,
    content: libraryPage(lib),
    source: `vyne/modules/external/${lib.id}/README.md`,
  });
}

// --- write content ----------------------------------------------------------
fs.mkdirSync(CONTENT, { recursive: true });
const seen = new Set();
for (const page of pages) {
  if (seen.has(page.id)) die(`Duplicate page id: ${page.id}`);
  seen.add(page.id);
  fs.writeFileSync(path.join(CONTENT, `${page.id}.html`), page.content.endsWith("\n") ? page.content : page.content + "\n");
}

// --- nav.json ---------------------------------------------------------------
const groupTitle = new Map(GROUPS);
const groupOrder = new Map(GROUPS.map(([title], i) => [title, i]));

for (const page of pages) {
  if (!groupTitle.has(page.group)) {
    groupTitle.set(page.group, "fa-file-lines");
    groupOrder.set(page.group, GROUPS.length + 1);
  }
}

const nav = { groups: [] };
const grouped = new Map();
for (const page of pages) {
  if (!grouped.has(page.group)) grouped.set(page.group, []);
  grouped.get(page.group).push(page);
}
for (const [title, items] of [...grouped.entries()].sort(
  (a, b) => (groupOrder.get(a[0]) ?? 999) - (groupOrder.get(b[0]) ?? 999),
)) {
  items.sort((a, b) => a.order - b.order || a.title.localeCompare(b.title));
  nav.groups.push({
    title,
    items: items.map((p) => ({ id: p.id, title: p.title, icon: p.icon })),
  });
}
fs.writeFileSync(NAV_FILE, JSON.stringify(nav, null, 2) + "\n");

// Remove content fragments that are no longer part of the site. This keeps the
// retired game/audio/server modules from lingering in the tree.
const removed = [];
for (const file of fs.readdirSync(CONTENT)) {
  if (!file.endsWith(".html")) continue;
  const id = file.slice(0, -5);
  if (!seen.has(id)) {
    fs.unlinkSync(path.join(CONTENT, file));
    removed.push(file);
  }
}

console.log(`\n✔ Synced ${pages.length} content fragments (${LIBRARIES.length} generated library pages)`);
if (removed.length) console.log(`  Removed stale content: ${removed.join(", ")}`);

if (NO_BUILD) process.exit(0);

const build = spawnSync(process.execPath, [path.join(__dirname, "build.mjs"), "--clean", "--root", OUTPUT], {
  cwd: DOCS,
  stdio: "inherit",
});
process.exit(build.status ?? 1);
