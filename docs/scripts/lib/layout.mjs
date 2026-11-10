// ---------------------------------------------------------------------------
// Shared page layout. Every section is wrapped in this identical shell so the
// navigation, top bar, table of contents and search stay consistent.
// ---------------------------------------------------------------------------

import { escapeHtml, escapeAttr, stripHtml } from "./slug.mjs";

export const SITE = {
  name: "Vyne",
  tagline: "Language Reference",
  version: "v0.7.0",
};

// Inline brand mark: a gradient tile with a layered chevron "V" (the layers
// echo the region/arena memory model). Kept inline so there is no extra
// request and the gradient id stays local.
const LOGO_MARK = `<svg class="logo-mark" viewBox="0 0 36 36" aria-hidden="true" focusable="false">
          <defs>
            <linearGradient id="vyneMark" x1="0" y1="0" x2="0.45" y2="1">
              <stop offset="0" stop-color="#4df0de" />
              <stop offset="0.5" stop-color="#6cb6ff" />
              <stop offset="1" stop-color="#b48cff" />
            </linearGradient>
          </defs>
          <rect x="0.5" y="0.5" width="35" height="35" rx="10.5" fill="url(#vyneMark)" />
          <path d="M9.5 11 L18 24.5 L26.5 11" fill="none" stroke="#080a10" stroke-width="3.1" stroke-linecap="round" stroke-linejoin="round" />
          <path d="M13 11 L18 18.8 L23 11" fill="none" stroke="#080a10" stroke-opacity="0.38" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" />
        </svg>`;

// Flatten nav.json into an ordered list with section + group info.
export function flattenNav(nav) {
  const items = [];
  nav.groups.forEach((group, gi) => {
    group.items.forEach((item) => {
      items.push({
        ...item,
        group: group.title,
        groupIndex: gi,
        path: item.id === "intro" ? "index.html" : `pages/${item.id}.html`,
      });
    });
  });
  return items;
}

function navGroupsHtml(nav, currentId, rel) {
  return nav.groups
    .map((group) => {
      const links = group.items
        .map((item) => {
          const active = item.id === currentId;
          const path = item.id === "intro" ? "index.html" : `pages/${item.id}.html`;
          return `<li><a class="nav-link${active ? " active" : ""}" href="${escapeAttr(rel + path)}"${
            active ? ' aria-current="page"' : ""
          }><i class="fas ${escapeAttr(item.icon || "fa-file-lines")}"></i><span>${escapeHtml(
            item.title,
          )}</span></a></li>`;
        })
        .join("");
      return (
        '<div class="nav-group">' +
        `<div class="nav-group-title">${escapeHtml(group.title)}</div>` +
        `<ul class="nav-list">${links}</ul>` +
        "</div>"
      );
    })
    .join("");
}

function pagerHtml(nav, currentId, rel) {
  const flat = flattenNav(nav);
  const idx = flat.findIndex((it) => it.id === currentId);
  if (idx === -1) return "";
  const prev = flat[idx - 1];
  const next = flat[idx + 1];
  const link = (item, dir) => {
    if (!item) return '<span class="pager-spacer"></span>';
    const label = dir === "prev" ? "Previous" : "Next";
    const arrow =
      dir === "prev"
        ? '<i class="fas fa-arrow-left"></i>'
        : '<i class="fas fa-arrow-right"></i>';
    const path = item.id === "intro" ? "index.html" : `pages/${item.id}.html`;
    return `<a class="pager-link ${dir}" href="${escapeAttr(rel + path)}"><span class="pager-label">${
      dir === "prev" ? arrow : ""
    } ${label}</span><span class="pager-title">${escapeHtml(item.title)}${
      dir === "next" ? ` ${arrow}` : ""
    }</span></a>`;
  };
  return `<nav class="pager" aria-label="Section navigation">${link(
    prev,
    "prev",
  )}${link(next, "next")}</nav>`;
}

export function renderPage({ nav, item, group, content, rel = "" }) {
  const title = `${item.title} · ${SITE.name} ${SITE.tagline}`;
  const description = stripHtml(content).slice(0, 180);
  return `<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>${escapeHtml(title)}</title>
    <meta name="description" content="${escapeAttr(description)}" />
    <meta name="color-scheme" content="dark" />
    <link rel="icon" type="image/svg+xml" href="${rel}assets/favicon.svg" />
    <link rel="preconnect" href="https://fonts.googleapis.com" />
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin />
    <link
      href="https://fonts.googleapis.com/css2?family=Inter:opsz,wght@14..32,400;14..32,500;14..32,600;14..32,700;14..32,800&family=Sora:wght@600;700;800&family=JetBrains+Mono:wght@400;500;600&display=swap"
      rel="stylesheet"
    />
    <link
      rel="stylesheet"
      href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css"
    />
    <link rel="stylesheet" href="${rel}assets/styles.css" />
  </head>
  <body data-page="${escapeAttr(item.id)}" data-root="${escapeAttr(rel)}">
    <a class="skip-link" href="#content">Skip to content</a>
    <div class="progress" id="progressBar" aria-hidden="true"></div>
    <div class="sidebar-overlay" id="sidebarOverlay" hidden></div>

    <aside class="sidebar" id="sidebar" aria-label="Documentation navigation">
      <div class="sidebar-head">
        <a class="brand" href="${rel}index.html">
          <span class="brand-badge">${LOGO_MARK}</span>
          <span class="brand-text">vyne</span>
          <span class="brand-version">${SITE.version}</span>
        </a>
        <button class="icon-btn sidebar-close" id="sidebarClose" aria-label="Close navigation">
          <i class="fas fa-xmark"></i>
        </button>
      </div>
      <nav class="sidebar-nav">${navGroupsHtml(nav, item.id, rel)}</nav>
      <div class="sidebar-foot">
        <span class="dot"></span> Active development
      </div>
    </aside>

    <div class="main">
      <header class="topbar">
        <div class="topbar-left">
          <button class="icon-btn mobile-toggle" id="menuToggle" aria-label="Open navigation" aria-controls="sidebar">
            <i class="fas fa-bars"></i>
          </button>
          <nav class="breadcrumb" aria-label="Breadcrumb">
            <a href="${rel}index.html">${SITE.name}</a>
            <span class="crumb-sep">/</span>
            <span class="crumb-group">${escapeHtml(group)}</span>
            <span class="crumb-sep">/</span>
            <b>${escapeHtml(item.title)}</b>
          </nav>
        </div>
        <div class="topbar-right">
          <button class="search-trigger" id="searchTrigger" type="button">
            <i class="fas fa-magnifying-glass"></i>
            <span class="search-trigger-text">Search docs…</span>
            <kbd class="kbd">⌘K</kbd>
          </button>
        </div>
      </header>

      <div class="content-wrap">
        <main class="content" id="content">
          <article class="page active">
            ${content}
          </article>
          ${pagerHtml(nav, item.id, rel)}
          <footer class="page-footer">
            <span>${SITE.name} ${SITE.version} — ${SITE.tagline}</span>
            <a href="${rel}index.html">Back to top</a>
          </footer>
        </main>

        <aside class="toc" id="toc" aria-label="On this page">
          <div class="toc-inner">
            <div class="toc-title">On this page</div>
            <ul class="toc-list" id="tocList"></ul>
          </div>
        </aside>
      </div>
    </div>

    <div class="palette" id="palette" hidden>
      <div class="palette-backdrop" data-close></div>
      <div class="palette-panel" role="dialog" aria-modal="true" aria-label="Search documentation">
        <div class="palette-input-wrap">
          <i class="fas fa-magnifying-glass"></i>
          <input id="paletteInput" type="text" placeholder="Search the docs…" autocomplete="off" spellcheck="false" />
          <kbd class="kbd">esc</kbd>
        </div>
        <ul class="palette-results" id="paletteResults"></ul>
        <div class="palette-foot">
          <span><kbd>↑</kbd><kbd>↓</kbd> navigate</span>
          <span><kbd>↵</kbd> open</span>
          <span><kbd>esc</kbd> close</span>
        </div>
      </div>
    </div>

    <button class="back-to-top" id="backToTop" type="button" aria-label="Back to top">
      <i class="fas fa-arrow-up"></i>
    </button>

    <script src="${rel}assets/search-index.js"></script>
    <script src="${rel}assets/app.js"></script>
  </body>
</html>
`;
}
