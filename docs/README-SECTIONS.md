# Vyne Docs — structure & the Markdown section generator

This site is a set of static HTML pages generated from small content files.
Adding a documentation page is a single command. The design, sidebar, table of
contents, search and pager are all applied automatically.

## Quick start

```bash
# regenerate content/ and nav.json from src/ + library manuals, then build
npm run sync

# build only (after editing content/*.html or nav.json by hand)
npm run build

# add one Markdown page through the single-section helper
npm run add -- --file docs/guide/threads.md --title "Threads" --group "Language Reference"

# preview locally (needs any static server; file:// also works)
npm run serve        # http://localhost:8000
```

Positional shorthand is also accepted:

```bash
node scripts/add-section.mjs docs/guide/threads.md "Threads" "Language Reference"
```

## Source layout

The maintained sources live under `docs/src/`. Each file carries a small
front-matter block:

```markdown
---
id: regions
title: Memory & Regions
group: Language Reference
icon: fa-layer-group
order: 9
description: One-line lede.
---
```

`.md` files are rendered with the site Markdown pipeline. `.html` files are
inserted verbatim and are used for the introduction hero and image-rich pages.
Library reference pages are generated from
`vyne/modules/external/<lib>/README.md`, so they never drift from the library
manuals. `npm run sync` writes `content/*.html`, updates `nav.json`, deletes
retired fragments, and rebuilds every page.

## What the `add` command does

1. Reads your Markdown file.
2. Converts it to the site's styled HTML.
3. Writes `content/<id>.html`.
4. Registers the section in `nav.json` (creating the group if needed).
5. Rebuilds **every** page so the sidebar, pager and search index include it.

It is safe to run repeatedly: running it again for the same file **updates** the
existing section instead of creating a duplicate.

## `add-section.mjs` options

| Flag | Positional | What it does |
| --- | --- | --- |
| `--file PATH` | 1st arg | **Required.** Path to the Markdown file. |
| `--title TEXT` | 2nd arg | Sidebar / page title. Falls back to front-matter, then the first `# heading`, then the filename. |
| `--group TEXT` | 3rd arg | Sidebar group. Falls back to front-matter, then `Docs`. |
| `--icon CLASS` | – | Font Awesome icon without the `fa-` prefix, e.g. `microchip`. Falls back to a group-based default. |
| `--order N` | – | 1-based position inside the group (defaults to append). |
| `--id SLUG` | – | URL/file id. Defaults to the slugified filename. |
| `--description TEXT` | – | Short lede shown under the title (also used for the meta description). |

### Front matter (optional)

Put this at the very top of your `.md` file; flags override it:

```markdown
---
title: Threads
group: Language Reference
icon: diagram-project
order: 3
description: Lightweight cooperative tasks built on top of regions.
---

# Threads

Your content…
```

## Supported Markdown

- Headings `#` … `####` — an H1 becomes the page title; H2/H3 feed the "On this
  page" table of contents and the search index.
- Paragraphs, **bold**, *italic* / _italic_, `inline code`.
- Links: `[text](https://example.com)` and internal links
  `[Memory & Regions](page:regions)` (jumps to another section by id).
- Ordered and unordered lists, including nested lists.
- GFM pipe tables.
- Fenced code blocks with highlighting:

  ````markdown
  ```vyne title="main.vy"
  module vcore;
  out("hello");
  ```

  ```shell
  vynec main.vy ./main
  ```
  ````

  `vyne` gets full syntax highlighting; `shell` / `bash` / `sh` / `console` get
  shell styling; any other language is shown as plain text. Every block gets a
  filename, language badge and copy button.

- Admonitions:

  ```markdown
  !!! note
  !!! tip "Optional custom title"
  !!! warning
  !!! danger
  ```

  The body is the following block indented by four spaces.

- Blockquotes (`>`) are rendered as note callouts.
- Horizontal rules (`---`).

## Project layout

```
index.html                 Introduction (generated)
pages/<id>.html            one generated page per section
content/<id>.html          generated fragment for each section
src/                       maintained Markdown/HTML sources
assets/styles.css          the entire design system
assets/app.js              progress bar, drawer, TOC, copy, ⌘K search
assets/search-index.js     generated search data
assets/srcnn/              showcase images
nav.json                   the sidebar/group/order manifest (generated)
scripts/sync-docs.mjs      src/ + library manuals -> content/ + nav.json
scripts/build.mjs          rebuild all pages + search index
scripts/add-section.mjs    the one-page Markdown -> section CLI
scripts/extract-legacy.mjs one-time importer (already used)
scripts/lib/               markdown, highlighter, layout, normalizer
```

## How to edit things

- **An existing page:** edit `src/<group>/<id>.md` (or `.html`), then
  `npm run sync`. Do not edit `content/` by hand; it is regenerated.
- **Sidebar order / icons / groups:** edit the front matter (`group`, `order`,
  `icon`) or the `LIBRARIES` manifest in `scripts/sync-docs.mjs`, then
  `npm run sync`.
- **A library reference:** edit the library's
  `vyne/modules/external/<lib>/README.md`, then `npm run sync`.
- **A quick one-off page:** `npm run add -- --file ...` still works, but the
  next `sync` will not know about it unless it is moved under `src/`.
- **Look & feel:** edit `assets/styles.css` (`:root` holds the design tokens).

## Notes on the original content

The original single-file `index.html` was split losslessly when the site was
first generated. The current site is a rewritten manual: the language surface,
region policy matrix, library set, CLI, and showcase track the v0.7.0 tree and
the unreleased `main` features (autodiff, `@realtime`, `@layout<soa>`,
`voptim`/`vsparse`/`vdecomp`, CUDA conv, and the SRCNN project).

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `⚙ Build failed` / `Missing content file` | `nav.json` lists an id with no `content/<id>.html`; run `add` for it or remove the entry. |
| Section not in the sidebar | Ensure it is listed in `nav.json`, then `npm run build`. |
| Changes to a `.md` don't appear | Re-run `npm run add` (the `build` alone won't re-read Markdown). |
| Want real URLs when opening files directly | Open `index.html` in a browser; internal links are relative and work from `file://`. |
