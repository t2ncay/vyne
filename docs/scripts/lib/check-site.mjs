import fs from "node:fs";
import path from "node:path";
import { flattenNav } from "./layout.mjs";

export function generatedFiles(root) {
  const files = ["index.html", "nav.json", "assets/search-index.js"];
  for (const dir of ["content", "pages"]) {
    const absolute = path.join(root, dir);
    if (!fs.existsSync(absolute)) continue;
    for (const entry of fs.readdirSync(absolute)) {
      if (entry.endsWith(".html")) files.push(`${dir}/${entry}`);
    }
  }
  return files.sort();
}

export function compareGenerated(expected, actual) {
  const errors = [];
  const files = new Set([...generatedFiles(expected), ...generatedFiles(actual)]);
  for (const file of files) {
    const source = path.join(expected, file);
    const target = path.join(actual, file);
    if (!fs.existsSync(source)) errors.push(`Retired generated file: ${file}`);
    else if (!fs.existsSync(target)) errors.push(`Missing generated file: ${file}`);
    else if (fs.readFileSync(source, "utf8").replace(/\r\n/g, "\n") !==
             fs.readFileSync(target, "utf8").replace(/\r\n/g, "\n")) {
      errors.push(`Stale generated file: ${file}`);
    }
  }
  return errors;
}

export function validateSite(root, assetRoot = root) {
  const errors = [];
  const nav = JSON.parse(fs.readFileSync(path.join(root, "nav.json"), "utf8"));
  const flat = flattenNav(nav);
  const seen = new Set();
  const htmlFiles = new Map();
  const anchors = new Map();
  for (const item of flat) {
    if (seen.has(item.id)) errors.push(`Duplicate navigation id: ${item.id}`);
    seen.add(item.id);
    const file = path.join(root, item.path);
    if (!fs.existsSync(file)) {
      errors.push(`Missing page: ${item.path}`);
      continue;
    }
    const html = fs.readFileSync(file, "utf8");
    htmlFiles.set(file, html);
    const ids = new Set();
    for (const match of html.matchAll(/\bid="([^"]+)"/g)) {
      if (ids.has(match[1])) errors.push(`Duplicate HTML id: ${item.path}#${match[1]}`);
      ids.add(match[1]);
    }
    anchors.set(file, ids);
  }

  for (const [file, html] of htmlFiles) {
    const from = path.relative(root, file);
    if (/\bdata-page="[^"]+"/.test(html.replace(/<body\b[^>]*>/, ""))) {
      errors.push(`Unresolved page link: ${from}`);
    }
    for (const match of html.matchAll(/<(?:a|link|script|img)\b[^>]*\b(?:href|src)="([^"]+)"/g)) {
      const href = match[1].replace(/&amp;/g, "&");
      if (/^(?:[a-z][a-z\d+.-]*:|\/\/)/i.test(href)) continue;
      const [resource, fragment] = href.split("#", 2);
      const pathname = resource.split("?", 1)[0];
      const target = pathname ? path.resolve(path.dirname(file), decodeURIComponent(pathname)) : file;
      const relative = path.relative(root, target);
      if (relative.startsWith("..") || path.isAbsolute(relative)) {
        errors.push(`Link escapes site: ${from} -> ${href}`);
        continue;
      }
      if (!fs.existsSync(target) && !fs.existsSync(path.join(assetRoot, relative))) {
        errors.push(`Missing local resource: ${from} -> ${href}`);
      } else if (fragment && anchors.has(target) && !anchors.get(target).has(decodeURIComponent(fragment))) {
        errors.push(`Missing anchor: ${from} -> ${href}`);
      }
    }
  }

  const searchFile = path.join(root, "assets/search-index.js");
  const search = fs.readFileSync(searchFile, "utf8").match(/window\.VYNE_SEARCH = ([\s\S]*);\s*$/);
  if (!search) errors.push("Search index has no VYNE_SEARCH data");
  else {
    const entries = JSON.parse(search[1]);
    const indexed = new Set();
    for (const entry of entries) {
      if (indexed.has(entry.id)) errors.push(`Duplicate search entry: ${entry.id}`);
      indexed.add(entry.id);
      const item = flat.find((page) => page.id === entry.id);
      if (!item || item.path !== entry.path) errors.push(`Invalid search target: ${entry.id}`);
      for (const heading of entry.headings || []) {
        if (!anchors.get(path.join(root, entry.path))?.has(heading.id)) {
          errors.push(`Invalid search heading: ${entry.id}#${heading.id}`);
        }
      }
    }
    for (const item of flat) {
      if (!indexed.has(item.id)) errors.push(`Page missing from search: ${item.id}`);
    }
  }
  return errors;
}
