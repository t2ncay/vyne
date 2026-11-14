#!/usr/bin/env node
// Rebuild outside the checkout, compare outputs, and validate published links.
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { compareGenerated, validateSite } from "./lib/check-site.mjs";

const scripts = path.dirname(fileURLToPath(import.meta.url));
const docs = path.resolve(scripts, "..");
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), "vyne-docs-check-"));
try {
  const result = spawnSync(process.execPath, [path.join(scripts, "sync-docs.mjs"), "--output-dir", temporary], {
    encoding: "utf8",
  });
  if (result.status !== 0) {
    console.error(result.stdout, result.stderr);
    process.exitCode = 1;
  } else {
    const errors = [...compareGenerated(temporary, docs), ...validateSite(temporary, docs)];
    if (errors.length) {
      console.error(errors.join("\n"));
      console.error("\nRun npm run sync in docs/ and commit the generated outputs.");
      process.exitCode = 1;
    } else console.log("Documentation is fresh; navigation, anchors, assets, and search targets are valid.");
  }
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
