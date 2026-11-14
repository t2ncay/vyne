import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import test from "node:test";
import { compareGenerated, validateSite } from "../lib/check-site.mjs";
import { rewriteRepositoryLinks } from "../lib/repository-links.mjs";

const scripts = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");

function fixture(t) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "vyne-site-test-"));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  fs.mkdirSync(path.join(dir, "content"));
  fs.mkdirSync(path.join(dir, "assets"));
  fs.writeFileSync(path.join(dir, "nav.json"), JSON.stringify({ groups: [
    { title: "Getting Started", items: [{ id: "intro", title: "Intro" }] },
    { title: "Reference", items: [{ id: "guide", title: "Guide" }] },
  ] }));
  fs.writeFileSync(path.join(dir, "content/intro.html"), '<h2 id="start">Start</h2><a data-page="guide#details">Guide</a>');
  fs.writeFileSync(path.join(dir, "content/guide.html"), '<h2 id="details">Details</h2><a data-page="intro#start">Home</a>');
  for (const asset of ["favicon.svg", "styles.css", "app.js"]) {
    fs.writeFileSync(path.join(dir, "assets", asset), "");
  }
  return dir;
}

function build(dir) {
  return spawnSync(process.execPath, [path.join(scripts, "build.mjs"), "--root", dir, "--clean"], { encoding: "utf8" });
}

test("repository manuals keep page links, fragments, and repository downloads after relocation", () => {
  const root = path.resolve("repository");
  const source = path.join(root, "vyne/modules/external/vraylib/README.md");
  const files = new Map([[path.join(path.dirname(source), "MANAGED.md"), "vraylib-managed"]]);
  const markdown = '[Managed](MANAGED.md#start) [API](../../../runtime/vraylib/api.json) [Web](https://example.com) [Here](#local)';
  const rewritten = rewriteRepositoryLinks(markdown, source, root, files);
  assert.match(rewritten, /\[Managed\]\(page:vraylib-managed#start\)/);
  assert.match(rewritten, /https:\/\/github.com\/t2ncay\/vyne\/blob\/master\/vyne\/runtime\/vraylib\/api.json/);
  assert.match(rewritten, /\[Web\]\(https:\/\/example.com\) \[Here\]\(#local\)/);
  assert.throws(() => rewriteRepositoryLinks('[Bad](../../../../../outside.md)', source, root), /escapes repository/);
});

test("generated navigation links and fragments work from both root and nested pages", (t) => {
  const dir = fixture(t);
  const result = build(dir);
  assert.equal(result.status, 0, result.stderr);
  assert.match(fs.readFileSync(path.join(dir, "index.html"), "utf8"), /href="pages\/guide.html#details"/);
  assert.match(fs.readFileSync(path.join(dir, "pages/guide.html"), "utf8"), /href="\.\.\/index.html#start"/);
  assert.deepEqual(validateSite(dir), []);
});

test("unknown internal page ids fail the build instead of publishing a broken URL", (t) => {
  const dir = fixture(t);
  fs.writeFileSync(path.join(dir, "content/guide.html"), '<a data-page="missing">Missing</a>');
  const result = build(dir);
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /Unknown internal page: missing/);
});

test("site validation detects missing anchors, assets, and incomplete search coverage", (t) => {
  const dir = fixture(t);
  assert.equal(build(dir).status, 0);
  const page = path.join(dir, "pages/guide.html");
  fs.appendFileSync(page, '<a href="../index.html#absent">Bad anchor</a><img src="../assets/missing.png">');
  fs.writeFileSync(path.join(dir, "assets/search-index.js"), 'window.VYNE_SEARCH = [];\n');
  const errors = validateSite(dir);
  assert.ok(errors.some((error) => error.includes("Missing anchor")));
  assert.ok(errors.some((error) => error.includes("Missing local resource")));
  assert.ok(errors.some((error) => error.includes("Page missing from search: guide")));
});

test("freshness comparison catches stale, missing, and retired generated output without editing it", (t) => {
  const expected = fixture(t);
  const actual = fixture(t);
  assert.equal(build(expected).status, 0);
  assert.equal(build(actual).status, 0);
  assert.deepEqual(compareGenerated(expected, actual), []);
  fs.appendFileSync(path.join(actual, "index.html"), "stale");
  fs.unlinkSync(path.join(actual, "pages/guide.html"));
  fs.writeFileSync(path.join(actual, "pages/retired.html"), "retired");
  const errors = compareGenerated(expected, actual);
  assert.ok(errors.includes("Stale generated file: index.html"));
  assert.ok(errors.includes("Missing generated file: pages/guide.html"));
  assert.ok(errors.includes("Retired generated file: pages/retired.html"));
  assert.ok(fs.readFileSync(path.join(actual, "index.html"), "utf8").endsWith("stale"));
});
