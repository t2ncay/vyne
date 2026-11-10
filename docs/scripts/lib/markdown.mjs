// ---------------------------------------------------------------------------
// A small, dependency-free Markdown -> site-HTML converter.
//
// Supported:
//   # .. #### headings (H2+ get unique ids for the TOC)
//   paragraphs, **bold**, *italic*/_italic_, `inline code`, [links](url),
//   internal links via [text](page:section-id)
//   ordered + unordered (nested) lists
//   GFM pipe tables
//   fenced code blocks (```vyne title="main.vy") with syntax highlighting
//   blockquotes (>) rendered as note admonitions
//   admonitions: !!! tip "Title" / !!! warning / !!! danger / !!! note
//   horizontal rules
// ---------------------------------------------------------------------------

import { uniqueSlug, escapeHtml, escapeAttr } from "./slug.mjs";
import { highlightVyne } from "./vyne-highlight.mjs";

export function parseFrontMatter(source) {
  const m = source.match(/^\uFEFF?---\r?\n([\s\S]*?)\r?\n---\r?\n?/);
  if (!m) return { data: {}, body: source };
  const data = {};
  for (const line of m[1].split(/\r?\n/)) {
    const kv = line.match(/^([A-Za-z0-9_-]+)\s*:\s*(.*)$/);
    if (!kv) continue;
    let value = kv[2].trim();
    if (/^["'].*["']$/.test(value)) value = value.slice(1, -1);
    data[kv[1].toLowerCase()] = value;
  }
  return { data, body: source.slice(m[0].length) };
}

// Pull the first H1 out of the body to use as the page title.
export function extractTitle(body) {
  const lines = body.split(/\r?\n/);
  for (let i = 0; i < lines.length; i++) {
    if (/^\s*$/.test(lines[i])) continue;
    const h1 = lines[i].match(/^#\s+(.*)$/);
    if (h1) {
      lines.splice(i, 1);
      return { title: h1[1].trim(), body: lines.join("\n") };
    }
    break;
  }
  return { title: "", body };
}

function shellHighlight(code) {
  return escapeHtml(code).replace(
    /(#.*)$/gm,
    (m) => `<span class="c-com">${m}</span>`,
  );
}

function inline(text) {
  const codes = [];
  text = text.replace(/`([^`]+)`/g, (_, c) => {
    codes.push(c);
    return `\u0000${codes.length - 1}\u0000`;
  });

  let html = escapeHtml(text);

  html = html.replace(/!\[([^\]]*)\]\(([^)]+)\)/g, (_, alt, src) => {
    return `<img src="${escapeAttr(src)}" alt="${escapeAttr(alt)}" loading="lazy">`;
  });

  html = html.replace(/\[([^\]]+)\]\(([^)]+)\)/g, (_, label, href) => {
    if (/^page:/.test(href)) {
      return `<a data-page="${escapeAttr(href.slice(5))}">${label}</a>`;
    }
    const external = /^https?:/.test(href);
    const attrs = external ? ' target="_blank" rel="noopener"' : "";
    return `<a href="${escapeAttr(href)}"${attrs}>${label}</a>`;
  });

  html = html.replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>");
  html = html.replace(/\*([^*]+)\*/g, "<em>$1</em>");
  html = html.replace(/(^|[\s(])_([^_]+)_(?=$|[\s).,;:!?])/g, "$1<em>$2</em>");

  html = html.replace(/\u0000(\d+)\u0000/g, (_, i) => {
    return `<code class="inline">${escapeHtml(codes[Number(i)])}</code>`;
  });
  return html;
}

function renderCodeBlock(info, code) {
  const parts = info.trim().split(/\s+/).filter(Boolean);
  let lang = "";
  let filename = "";
  for (const p of parts) {
    const t = p.match(/^(?:title|filename|file)=(.*)$/i);
    if (t) filename = t[1].replace(/^["']|["']$/g, "");
    else if (!lang) lang = p;
  }
  if (!filename) filename = lang ? `${lang} snippet` : "snippet";
  const lc = (lang || "text").toLowerCase();
  let body;
  if (["vyne", "vy", "v"].includes(lc)) body = highlightVyne(code);
  else if (["shell", "bash", "sh", "console", "zsh"].includes(lc))
    body = shellHighlight(code);
  else body = escapeHtml(code);

  const icon = /shell|bash|sh|console|zsh/.test(lc)
    ? "fas fa-terminal"
    : "far fa-file-code";
  return (
    '<div class="code-block"><div class="code-header">' +
    `<span class="code-filename"><i class="${icon}"></i>&nbsp;${escapeHtml(filename)}</span>` +
    `<span class="code-lang">${escapeHtml(lang || "text")}</span>` +
    '<button class="copy-btn" type="button"><i class="far fa-copy"></i> copy</button>' +
    `</div><div class="code-body" data-lang="${escapeAttr(lang)}">${body}</div></div>`
  );
}

const ADM = {
  note: { cls: "adm-note", icon: "fa-circle-info", title: "Note" },
  info: { cls: "adm-note", icon: "fa-circle-info", title: "Note" },
  tip: { cls: "adm-tip", icon: "fa-lightbulb", title: "Tip" },
  success: { cls: "adm-tip", icon: "fa-circle-check", title: "Success" },
  warning: { cls: "adm-warn", icon: "fa-triangle-exclamation", title: "Warning" },
  warn: { cls: "adm-warn", icon: "fa-triangle-exclamation", title: "Warning" },
  danger: { cls: "adm-danger", icon: "fa-circle-exclamation", title: "Danger" },
  error: { cls: "adm-danger", icon: "fa-circle-exclamation", title: "Danger" },
};

function renderAdmonition(type, title, innerHtml) {
  const conf = ADM[type] || ADM.note;
  const heading = title || conf.title;
  return (
    `<div class="admonition ${conf.cls}">` +
    `<i class="fas ${conf.icon}"></i>` +
    '<div class="adm-body">' +
    `<div class="adm-title">${inline(heading)}</div>` +
    innerHtml +
    "</div></div>"
  );
}

function isTableSeparator(line) {
  return /^\s*\|?\s*:?-{2,}:?\s*(\|\s*:?-{2,}:?\s*)+\|?\s*$/.test(line);
}
function splitRow(line) {
  const s = line.trim().replace(/^\|/, "").replace(/\|$/, "");
  const cells = [];
  let current = "";
  let inCode = false;
  for (let i = 0; i < s.length; i++) {
    const ch = s[i];
    if (ch === "\\" && s[i + 1] === "|") {
      current += "|";
      i++;
      continue;
    }
    if (ch === "`") {
      inCode = !inCode;
      current += ch;
      continue;
    }
    if (ch === "|" && !inCode) {
      cells.push(current.trim());
      current = "";
      continue;
    }
    current += ch;
  }
  cells.push(current.trim());
  return cells;
}
function isTableStart(lines, i) {
  return (
    lines[i] &&
    lines[i].includes("|") &&
    lines[i + 1] &&
    isTableSeparator(lines[i + 1])
  );
}

function parseList(lines, start) {
  const first = lines[start].match(/^(\s*)([-*+]|\d+\.)\s+(.*)$/);
  const baseIndent = first[1].length;
  const ordered = /\d/.test(first[2]);
  const items = [];
  let i = start;
  while (i < lines.length) {
    const m = lines[i].match(/^(\s*)([-*+]|\d+\.)\s+(.*)$/);
    if (!m) break;
    const indent = m[1].length;
    if (indent < baseIndent) break;
    if (indent > baseIndent) {
      const sub = parseList(lines, i);
      items[items.length - 1].children += sub.html;
      i = sub.next;
      continue;
    }
    const item = { text: m[3], children: "" };
    items.push(item);
    i++;
    while (
      i < lines.length &&
      lines[i].trim() !== "" &&
      !/^\s*([-*+]|\d+\.)\s+/.test(lines[i]) &&
      /^\s+\S/.test(lines[i])
    ) {
      item.text += " " + lines[i].trim();
      i++;
    }
  }
  const tag = ordered ? "ol" : "ul";
  const body = items
    .map((it) => `<li>${inline(it.text)}${it.children}</li>`)
    .join("");
  return { html: `<${tag}>${body}</${tag}>`, next: i };
}

export function renderMarkdown(source) {
  const lines = source.replace(/\r\n/g, "\n").split("\n");
  const used = new Set();
  const out = [];
  let i = 0;

  while (i < lines.length) {
    const line = lines[i];

    if (/^\s*$/.test(line)) {
      i++;
      continue;
    }

    // fenced code
    const fence = line.match(/^\s*```(.*)$/);
    if (fence) {
      const info = fence[1];
      const buf = [];
      i++;
      while (i < lines.length && !/^\s*```\s*$/.test(lines[i])) {
        buf.push(lines[i]);
        i++;
      }
      i++; // closing fence
      out.push(renderCodeBlock(info, buf.join("\n")));
      continue;
    }

    // admonition
    const adm = line.match(/^!!!\s+([A-Za-z]+)\s*(?:"([^"]*)")?\s*$/);
    if (adm) {
      const type = adm[1].toLowerCase();
      const title = adm[2] || "";
      const buf = [];
      i++;
      while (i < lines.length) {
        if (/^\s*$/.test(lines[i])) {
          buf.push("");
          i++;
          continue;
        }
        if (/^(?: {4}|\t)/.test(lines[i])) {
          buf.push(lines[i].replace(/^(?: {4}|\t)/, ""));
          i++;
        } else break;
      }
      const inner = renderMarkdown(buf.join("\n"));
      out.push(renderAdmonition(type, title, inner));
      continue;
    }

    // blockquote -> note admonition
    if (/^>\s?/.test(line)) {
      const buf = [];
      while (i < lines.length && /^>\s?/.test(lines[i])) {
        buf.push(lines[i].replace(/^>\s?/, ""));
        i++;
      }
      const inner = renderMarkdown(buf.join("\n"));
      out.push(renderAdmonition("note", "", inner));
      continue;
    }

    // horizontal rule
    if (/^\s*([-*_])\s*(\1\s*){2,}$/.test(line)) {
      out.push("<hr>");
      i++;
      continue;
    }

    // headings
    const h = line.match(/^(#{1,6})\s+(.*)$/);
    if (h) {
      const level = h[1].length;
      const text = h[2].trim();
      if (level === 1) {
        out.push(`<h1>${inline(text)}</h1>`);
      } else {
        const id = uniqueSlug(text, used);
        out.push(`<h${level} id="${id}">${inline(text)}</h${level}>`);
      }
      i++;
      continue;
    }

    // table
    if (isTableStart(lines, i)) {
      const headers = splitRow(lines[i]);
      i += 2;
      const rows = [];
      while (i < lines.length && lines[i].includes("|") && lines[i].trim() !== "") {
        rows.push(splitRow(lines[i]));
        i++;
      }
      const thead = `<thead><tr>${headers
        .map((c) => `<th>${inline(c)}</th>`)
        .join("")}</tr></thead>`;
      const tbody = `<tbody>${rows
        .map(
          (r) =>
            `<tr>${r.map((c) => `<td>${inline(c)}</td>`).join("")}</tr>`,
        )
        .join("")}</tbody>`;
      out.push(`<div class="table-wrap"><table>${thead}${tbody}</table></div>`);
      continue;
    }

    // list
    if (/^\s*([-*+]|\d+\.)\s+/.test(line)) {
      const list = parseList(lines, i);
      out.push(list.html);
      i = list.next;
      continue;
    }

    // paragraph
    const buf = [line.trim()];
    i++;
    while (
      i < lines.length &&
      lines[i].trim() !== "" &&
      !/^\s*```/.test(lines[i]) &&
      !/^#{1,6}\s/.test(lines[i]) &&
      !/^>\s?/.test(lines[i]) &&
      !/^!!!\s/.test(lines[i]) &&
      !/^\s*([-*+]|\d+\.)\s+/.test(lines[i]) &&
      !isTableStart(lines, i)
    ) {
      buf.push(lines[i].trim());
      i++;
    }
    out.push(`<p>${inline(buf.join(" "))}</p>`);
  }

  return out.join("\n");
}
