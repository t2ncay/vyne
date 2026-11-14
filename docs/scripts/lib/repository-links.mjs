// Resolve links before a repository manual moves into docs/pages/.
import path from "node:path";

export function rewriteRepositoryLinks(markdown, sourceFile, root, siteFiles = new Map()) {
  return markdown.replace(/(!?\[[^\]]*\])\(([^)]+)\)/g, (match, label, href) => {
    if (/^(?:[a-z][a-z\d+.-]*:|#|\/\/)/i.test(href)) return match;
    const [target, fragment] = href.split("#", 2);
    const absolute = path.resolve(path.dirname(sourceFile), target);
    const id = siteFiles.get(absolute);
    if (id && !label.startsWith("!")) {
      return `${label}(page:${id}${fragment ? `#${fragment}` : ""})`;
    }
    const relative = path.relative(root, absolute).split(path.sep).join("/");
    if (relative.startsWith("../")) throw new Error(`Manual link escapes repository: ${href}`);
    const url = `https://github.com/t2ncay/vyne/blob/master/${relative.split("/").map(encodeURIComponent).join("/")}`;
    return `${label}(${url}${fragment ? `#${fragment}` : ""})`;
  });
}
