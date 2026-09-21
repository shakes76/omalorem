# Omaview — Specification

Status: Draft v0.1 · 2026-09-21
Upstream: [omacom-io/omawrite](https://github.com/omacom-io/omawrite) (MIT), forked at `8f98892` (Omawrite 0.5.0)

## 1. Purpose

Omaview is Omawrite plus a live preview. You write Markdown in Omawrite's editor on the
left and see it rendered on the right, with LaTeX math typeset by KaTeX. It is meant for
notes, papers and technical writing where you need to check that the equations are right
while you write.

Omaview keeps Omawrite's character: one window, no toolbars, no sidebars, no settings
dialog, a narrow typographic column, and theme and text size taken from the desktop.
The preview should read as a second page of the same document, not as a browser tab.

## 2. Principles

1. **The editor is untouched.** Everything Omawrite does (smart return, link paste,
   recovery, external-change detection, search and replace, the wheel physics) keeps
   working exactly as it does upstream. The preview is added beside it. The fork should
   stay easy to rebase onto upstream Omawrite.
2. **Lightweight at rest.** Chromium (QtWebEngine) loads only the first time the preview
   is shown. If you never open the preview, startup time and memory stay the same as
   Omawrite's.
3. **Offline and sandboxed.** The preview never touches the network. Every asset
   (markdown-it, KaTeX, fonts, CSS) is compiled into the binary through `qrc`.
4. **Follows the desktop.** Omarchy theme colours, light and dark mode, and the text
   scale apply to the preview live, the same way they apply to the editor.
5. **Keyboard first.** Every preview feature has a shortcut. The footer gets at most one
   extra icon.

## 3. Scope

### In scope (v1)
- A split view with the editor and a live preview. There are three view modes:
  **Write** (editor only, the same as Omawrite), **Split**, and **Preview** (preview only,
  for reading).
- CommonMark and GFM rendering: tables, task lists, strikethrough, autolinks, fenced code.
- Math: `$…$` inline and `$$…$$` display, plus `\(…\)` and `\[…\]`. Inside display math,
  KaTeX environments such as `align`, `aligned`, `cases`, `pmatrix` and `gather` work.
- If an expression has a KaTeX error, only that expression is shown as an error. The rest
  of the document still renders.
- Scroll sync from the editor to the preview.
- Local images, resolved relative to the document's folder.
- Math insertion shortcuts, and math spans highlighted in the editor.
- Export to PDF with the math rendered, and printing through the preview.

### Out of scope (v1)
- Mermaid, diagrams and syntax-highlighted code in the preview. These may come later as
  vendored add-ons.
- Editing inside the preview.
- A settings UI. Preferences go in `QSettings` and are changed through shortcuts only.
- Remote content: remote images, web fonts and iframes.
- Windows and macOS. WebView2 is not used. The target is Linux on Wayland (Hyprland),
  with X11 as best effort.

## 4. User experience

### 4.1 Layout

```
┌─────────────────────────────── Omaview ───────────────────────────────┐
│                                   │                                   │
│   # Heat equation                 │   Heat equation                   │
│                                   │                                   │
│   The solution satisfies          │   The solution satisfies          │
│                                   │                                   │
│   $$                              │        ∂u        2                │
│   \partial_t u = \alpha \nabla^2 u│        ── = α ∇ u                 │
│   $$                              │        ∂t                         │
│                                   │                                   │
│  [save][open][◧] status            ┆                         42 Words │
└───────────────────────────────────┴───────────────────────────────────┘
```

- **Split:** the editor and preview each start at 50% width. The divider is a 1px
  `mutedColor` line with a wider invisible grab area. Dragging it changes the ratio,
  which is saved (`view/splitRatio`, clamped to 0.25–0.75). Each pane centres its own
  text column. The editor column follows the existing `editorWidth` rule, applied to
  the pane width instead of the window width.
- **Write:** exactly Omawrite. The WebEngineView still exists but is hidden, and it stops
  rendering after the first time it has been used.
- **Preview:** the preview fills the window. Keystrokes don't edit the text, but save,
  open, search, mode switching and the other shortcuts still work.
- If the window is narrower than about 900 logical px, Split becomes Write + Preview
  toggling. The saved mode is kept and comes back when the window is wide enough.
- The first launch opens in **Split**. After that, Omaview opens in the last mode used
  (`view/mode`).
- The footer gets one extra `FooterIconButton` (`iconName: "preview"`) that cycles the
  view mode. The word count stays at the bottom right of the window.

### 4.2 Shortcuts (added to Omawrite's)

| Shortcut | Action |
|---|---|
| `Ctrl+E` | Toggle Write ↔ Split |
| `Ctrl+Shift+E` | Toggle Preview-only ↔ previous mode |
| `Ctrl+M` | Wrap the selection in inline math `$…$`, or insert `$$` with the cursor inside |
| `Ctrl+Shift+M` | Insert a display math block `$$\n…\n$$` on its own lines |
| `Ctrl+Shift+P` | Export to PDF (portal save dialog, suggests `<name>.pdf`) |
| `Ctrl+P` | Print. Uses the rendered preview, so the math appears |

All of these are added to the `Ctrl+?` reference dialog and to the README.

### 4.3 Preview typography
- Prose uses **iA Writer Mono S**, the same as the editor, so the two panes match.
  Headings use weight and size only, never colour. Code uses the same font on a faint
  tinted background.
- The column is about 65ch, the same measure as the editor, and centred in the pane.
- Colours come from CSS custom properties fed by `backend.theme*`: `--bg`, `--fg`,
  `--accent` (links, blockquote rule), `--selection`, and `--muted`.
- The base font size is `20px × textScale`, matching `editorFontPixelSize`. It changes
  live on `textScaleChanged`.
- KaTeX uses its own bundled fonts and inherits `--fg`. Display math that is too wide
  scrolls horizontally inside its own block, so the page never scrolls sideways.

### 4.4 Behaviour
- **Live update:** the preview re-renders 120 ms after the last keystroke, the same
  interval as the word-count timer. Opening a file or reloading renders immediately.
- **Scroll position** stays put across re-renders. The page is never swapped out.
- **Scroll sync (editor → preview):** the block at the top of the editor viewport lines
  up with the matching block in the preview, using `data-source-line`. Preview → editor
  sync is v1.1.
- **Links:** clicking a link calls `backend.openExternalUrl` (http, https and mailto
  only). `#anchor` links scroll within the preview. The preview itself never navigates
  away.
- **Images:** a relative `src` resolves against the open file's folder. An unsaved
  document has no base folder, so its images show a placeholder. Remote images are
  blocked and show a small placeholder with the alt text.
- **Math errors:** KaTeX runs with `throwOnError: false`, so a bad expression is shown as
  source text in `--accent` with the error message as a tooltip.
- **Raw HTML** in the Markdown is escaped (markdown-it `html: false`). *(Open question 3.)*

## 5. Architecture

### 5.1 Rendering pipeline (decision)

```
TextEdit ──textChanged──▶ Backend (debounce 120ms) ──▶ PreviewBridge.markdown (Q_PROPERTY)
                                                              │ QWebChannel
                                                              ▼
                              qrc:/preview/index.html ── markdown-it + texmath → KaTeX
                                                              │
                                                     DOM patch (per top-level block)
```

**Markdown is parsed in the page with markdown-it**, using a KaTeX math plugin, and all of
it is vendored.

Why markdown-it:
- Its tokens record source line ranges (`token.map`). That gives `data-source-line`
  attributes for scroll sync with no extra work. VS Code's preview uses the same approach.
- The math plugins handle the edge cases around `$` (currency, escaping, code spans) and
  are well tested.
- The only C++ involved is a thin bridge, which keeps the fork's diff against upstream
  small.

Alternative considered: md4c-html (already present as a qt6-base dependency) with
`MD_FLAG_LATEXMATHSPANS`, emitting `<x-equation>` elements that KaTeX then renders. It
was rejected because md4c provides no source-line mapping, which rules out accurate
scroll sync. Keep it in mind if JS parsing ever becomes a bottleneck.

### 5.2 Components

| Unit | Kind | Responsibility |
|---|---|---|
| `PreviewBridge` (`src/previewbridge.{h,cpp}`) | `QObject`, registered on a `QWebChannel` | Properties: `markdown`, `baseUrl` (document folder as `file://…/`), `theme` (`QVariantMap`: bg, fg, accent, selection, muted, dark), `textScale`, `sourceLine` (for sync). Invokables called from the page: `openLink(url)`, `ready()`. Owns the debounce timer. |
| `Backend` | existing | Keeps its current API. Additions: a `previewBridge` property, and feeding text, theme and scale into the bridge. The `editorTextChanged()` path already knows when content has really changed, and that is what triggers the bridge. |
| `PreviewPane.qml` | new QML file | `Loader` → `WebEngineView` (created lazily), transparent background, `settings.javascriptCanOpenWindows: false`, `localContentCanAccessFileUrls: true`, `localContentCanAccessRemoteUrls: false`, `onNavigationRequested` blocks everything except the initial qrc load, and `onNewWindowRequested` sends the URL to `openLink`. |
| `SplitView` in `Main.qml` | QML | Wraps the existing editor `Flickable` and `PreviewPane`. The editor subtree moves unchanged. |
| `preview/index.html`, `preview.css`, `preview.js` | qrc assets | Rendering, DOM patching, theme variables, scroll sync, link interception. |
| `third_party/` | vendored JS, CSS and fonts | `markdown-it`, `markdown-it-texmath` (or `@vscode/markdown-it-katex`), `katex` (min.js, min.css, woff2 fonts only), `qwebchannel.js` (copied from Qt). Exact versions pinned in `third_party/VERSIONS`, with each licence alongside. |
| `RequestInterceptor` | `QWebEngineUrlRequestInterceptor` on an off-the-record profile | Allows only `qrc:` and `file:` under the document folder; blocks everything else. This enforces principle 3. |

### 5.3 Startup
- `main.cpp` calls `QtWebEngineQuick::initialize()` **before** `QApplication` is
  constructed. Qt requires this.
- The `WebEngineView` sits inside a `Loader` with `active: false` until the first time the
  mode is anything other than Write. After that it stays alive (`active` never goes
  back to false) so switching modes is instant.
- Target: with the preview hidden, cold start is no more than 10% slower than Omawrite's.
  With the preview shown, the first render appears within 600 ms of the window.

### 5.4 DOM update strategy
Each render builds the token stream and renders each top-level block to HTML, keyed by
`hash(block source)`. The page then patches `#content`: unchanged blocks keep their DOM
nodes (and their typeset KaTeX), changed blocks are replaced, and scroll position is kept.
Typing in one paragraph therefore re-typesets only that paragraph.
Budget: under 16 ms per update for a 2,000-line document with 200 equations, measured
on a 2024 laptop CPU.

### 5.5 Printing and PDF
- **Export PDF:** `WebEngineView.printToPdf(path, pageSize, orientation)`, with the path
  chosen through the existing portal `FileDialog` (Save mode, `*.pdf`). A print
  stylesheet (`@media print`) forces the light palette, removes the column centring,
  and uses A4 or Letter from the locale.
- **Print (`Ctrl+P`):** replaces Omawrite's `QTextDocument` path, which can't show math.
  It renders a temporary PDF, then sends it to the printer through `QPrintDialog`, using
  `QPdfDocument` (Qt PDF, already pulled in by qt6-webengine) to rasterise pages.
  If the preview has never been loaded, print loads it offscreen first.

### 5.6 Editor-side additions
- `MarkdownHighlighter` gets a math rule. `$…$`, `$$…$$`, `\(…\)` and `\[…\]` spans are
  drawn in `themeAccent` at reduced opacity, and the delimiters are muted like the other
  Markdown syntax. Math inside fenced code is left alone.
- `smartReturn` treats an open `$$` block like an open code fence: Return inserts a
  single newline, with no list continuation.

## 6. Build and packaging

- `omaview.pro`: add `webenginequick webchannel pdf` to `QT`, plus the new sources and a
  second qrc (`src/preview.qrc`) for the web assets.
- `PKGBUILD`: add `qt6-webengine` and `qt6-webchannel` to `depends`.
- `bin/build`, `bin/test` and `bin/install` stay as they are (qmake6 + make).
- The binary, desktop file, icon, `QSettings` app name and recovery directory are all
  named `omaview`. Omaview and Omawrite can be installed together without clashing.
- A qmake scope (`no_preview { … }`, enabled with `qmake6 CONFIG+=no_preview`) builds
  the editor without WebEngine, for debugging and for rebasing on upstream.

## 7. Testing

- **Existing suite:** all 12 inherited `QtTest` cases must keep passing (`bin/test`,
  offscreen).
- **Bridge unit tests (C++):** debounce coalescing, theme map contents after
  `colors.toml` changes, `baseUrl` for saved and unsaved documents, and link filtering
  (reject `javascript:` and `file:` outside the document folder).
- **Render tests:** a `tests/preview/` QtTest target loads `index.html` in an offscreen
  `QQuickWebEngineView` from a fixture corpus (`tests/fixtures/*.md`). It asserts, using
  `runJavaScript`: the number of `.katex` and `.katex-display` nodes, the absence of
  `.katex-error` in valid fixtures, the presence of error nodes in invalid ones, currency
  like `$5 and $10` not being treated as math, and math inside code spans or fences
  staying literal.
- **Performance test:** the 2,000-line fixture must meet the budget in 5.4. It is run
  manually and logged, not gated in CI.
- **Manual checklist:** Hyprland tiling, fractional scaling at 1.25 and 1.5, switching
  theme with `omarchy theme set …` and changing text size with `omarchy display text
  size …` while the app is open, dark and light toggle, and the portal dialogs for PDF
  export.

## 8. Milestones

| # | Deliverable | Acceptance |
|---|---|---|
| M0 | Fork and rename *(done)* | Builds as `omaview`, 12/12 tests pass, upstream remote kept |
| M1 | Split view with a static preview | `Ctrl+E` toggles modes. Markdown and math render from qrc with no network. Theme and scale apply live. Loads lazily. |
| M2 | Incremental render and scroll sync | Block-keyed DOM patching. The performance budget is met. Editor→preview sync works. Link and image handling is done. |
| M3 | Editor math support | Highlighter rule, `Ctrl+M` / `Ctrl+Shift+M`, and `$$`-aware smart return, each with tests |
| M4 | PDF export and print | `Ctrl+Shift+P` and `Ctrl+P` produce output with the math rendered |
| M5 | Packaging | PKGBUILD dependencies, desktop file, icon variant, README, `bin/install` works |

## 9. Open questions

1. **Preview body font:** keep Mono so both panes match, or use a proportional face
   (iA Writer Quattro is also OFL) so the preview reads more like print?
2. **A separate preview window:** Hyprland would tile a second top-level window beside the
   editor automatically, which suits multiple monitors. Should v1.1 add a
   "detach preview" option?
3. **Raw HTML:** stay escaped, or allow it through a sanitiser (DOMPurify)? Many
   READMEs use `<details>` and `<img width>`.
4. **Remote images:** blocked for privacy and to keep the app lightweight. Should there be
   a per-document opt-in?
5. **KaTeX macros:** read a `macros:` block from YAML front matter, or a
   `~/.config/omaview/macros.tex` file?
