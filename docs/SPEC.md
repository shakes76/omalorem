# Omaview — Specification

Status: Draft v0.3 · 2026-09-21
Upstream: [omacom-io/omawrite](https://github.com/omacom-io/omawrite) (MIT), forked at `8f98892` (Omawrite 0.5.0)

## 1. Purpose

Omaview is Omawrite plus a live preview. You write Markdown in Omawrite's editor, and a
separate preview window shows it rendered, with LaTeX math typeset by KaTeX. It is meant
for notes, papers and technical writing where you need to check that the equations are
right while you write.

Omaview keeps Omawrite's character: no toolbars, no sidebars, no settings dialog, a
narrow typographic column, and theme and text size taken from the desktop. Omaview is
designed first for **Omarchy**. By default the preview is its own top-level window, and
Hyprland tiles it beside the editor, so the tiling window manager handles the layout
rather than the app. A docked split layout is available for other desktops or for anyone
who prefers a single window.

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
4. **Native to the desktop.** Let the tiling window manager arrange the windows. Omarchy
   theme colours, light and dark mode, and the text scale apply live to both windows.
5. **Keyboard first.** Every preview feature has a shortcut, and the shortcuts work
   whichever window has focus. The footer gets at most one extra icon.
6. **Simple first.** Where a feature has a simple version and a flexible one (raw HTML,
   remote content), v1 ships the simple, safe version.

## 3. Scope

### In scope (v1)
- A live preview in its **own window by default**. Pressing `Ctrl+Shift+E` switches to a
  **docked** split inside the editor window. `Ctrl+E` shows or hides the preview in
  either placement.
- CommonMark and GFM rendering: tables, task lists, strikethrough, autolinks, fenced code.
- Math: `$…$` inline and `$$…$$` display, plus `\(…\)` and `\[…\]`. Inside display math,
  KaTeX environments such as `align`, `aligned`, `cases`, `pmatrix` and `gather` work.
- If an expression has a KaTeX error, only that expression is shown as an error. The rest
  of the document still renders.
- The preview font matches the editor (iA Writer Mono S). You can switch it to the
  proportional iA Writer Quattro S.
- Scroll sync from the editor to the preview.
- Local images, resolved relative to the document's folder.
- Math insertion shortcuts, and math spans highlighted in the editor.
- Export to PDF with the math rendered, and printing through the preview.

### Out of scope (v1)
- **Raw HTML.** It is always escaped and shown as text. There is no sanitiser and no
  opt-in.
- **Remote content.** Remote images, web fonts and iframes are always blocked, and there
  is no per-document opt-in.
- Mermaid, diagrams and syntax-highlighted code in the preview.
- Editing inside the preview.
- A settings UI. Preferences go in `QSettings` and are changed through shortcuts only.
- A dedicated preview-only mode. To read, fullscreen the preview window instead
  (`Super+F` / `F11`).
- Windows and macOS. WebView2 is not used. The target is Linux on Wayland (Hyprland),
  with X11 as best effort.

## 4. User experience

### 4.1 Preview placement

There are two placements, saved as `preview/placement` with the values `window` and
`docked`. The default is **`window`**. Whether the preview is showing is saved separately
as `preview/visible`, which defaults to `true`.

**Pop-out (default).** This is the view on Omarchy, with Hyprland tiling the two windows
side by side:

```
┌──────────── notes.md - Omaview ───────────┐┌──────── Preview — notes.md - Omaview ─────┐
│                                           ││                                           │
│   # Heat equation                         ││   Heat equation                           │
│                                           ││                                           │
│   The solution satisfies                  ││   The solution satisfies                  │
│                                           ││                                           │
│   $$                                      ││            ∂u        2                    │
│   \partial_t u = \alpha \nabla^2 u        ││            ── = α ∇ u                     │
│   $$                                      ││            ∂t                             │
│                                           ││                                           │
│  [save][open][◨] status          42 Words ││                                           │
└───────────────────────────────────────────┘└───────────────────────────────────────────┘
```

- The preview is a separate top-level `Window`. It has **no transient parent**, because
  Hyprland floats transient windows, and tiling is the point. Its title is
  `Preview — <fileName> - Omaview`, following the editor's title as it changes. Its
  background is `themeBackground` and it has no chrome of its own.
- **Order of opening.** On startup, and whenever `Ctrl+E` shows the preview, the preview
  window opens after the editor window and the editor keeps keyboard focus. The code
  calls `requestActivate()` on the editor after showing the preview. Hyprland may refuse
  that request depending on `misc:focus_on_activate`, so §6.1 also offers an optional
  window rule for users on Omarchy.
- **Closing.** Closing the editor window closes its preview. Closing the preview window
  (Super+W on Omarchy) only hides it and sets `preview/visible = false`. It does not
  affect the document or the unsaved-changes flow.
- **Several documents.** `Ctrl+N` starts a new process, as upstream does. Each editor has
  its own preview window.
- **Geometry.** Omaview does not save the preview window's size or position. Hyprland
  decides it when tiling. If the user floats the window, the compositor takes over.
- **Shortcuts in the preview.** Every `Qt.ApplicationShortcut` (save, open, print, find,
  `Ctrl+E`, and so on) works while the preview window has focus. Shortcuts that edit
  text (`Ctrl+B`, `Ctrl+Z`, …) do nothing there. Find, when started from the preview,
  focuses the editor window.

**Docked.** This is for desktops without tiling, or for a single-window workflow:

```
┌─────────────────────────────── Omaview ───────────────────────────────┐
│   # Heat equation                 │   Heat equation                   │
│   …                               │   …                               │
│  [save][open][◨] status            ┆                         42 Words │
└───────────────────────────────────┴───────────────────────────────────┘
```

- The editor and preview each start at 50% width. The divider is a 1px `mutedColor`
  line with a wider invisible grab area. Dragging it changes the ratio, which is saved
  (`preview/splitRatio`, clamped to 0.25–0.75). Each pane centres its own text column.
  The editor column follows the existing `editorWidth` rule, applied to the pane width
  instead of the window width.
- If the window is narrower than about 900 logical px, the docked preview hides. It comes
  back when the window is wide enough again.

**Switching placement** (`Ctrl+Shift+E`) destroys the current preview view and creates a
new one in the other placement (see §5.3). It then re-renders from the bridge's current
state and restores the scroll position from `sourceLine`.

The footer gets one extra `FooterIconButton` (`iconName: "preview"`) that shows or hides
the preview. The word count stays at the bottom right of the editor window.

### 4.2 Shortcuts (added to Omawrite's)

| Shortcut | Action |
|---|---|
| `Ctrl+E` | Show or hide the preview, in its current placement |
| `Ctrl+Shift+E` | Switch placement between pop-out window and docked |
| `Ctrl+Shift+T` | Switch the preview font between Mono and Quattro |
| `Ctrl+M` | Wrap the selection in inline math `$…$`, or insert `$$` with the cursor inside |
| `Ctrl+Shift+M` | Insert a display math block `$$\n…\n$$` on its own lines |
| `Ctrl+Shift+P` | Export to PDF (portal save dialog, suggests `<name>.pdf`) |
| `Ctrl+P` | Print. Uses the rendered preview, so the math appears |

All of these are added to the `Ctrl+?` reference dialog and to the README.

### 4.3 Preview typography
- **The font matches the editor by default.** Prose uses **iA Writer Mono S**, the font
  the editor already bundles.
- **Proportional option.** `Ctrl+Shift+T` switches prose to **iA Writer Quattro S**, the
  proportional version of the same family. It has the same OFL licence, and Omaview
  bundles Regular, Italic, Bold and BoldItalic. The choice is saved in `preview/font`
  (`mono` by default, or `quattro`). It applies straight away and doesn't touch the
  editor font. Code spans and fenced code always stay in Mono.
- Headings use weight and size only, never colour. Code uses the same font on a faint
  tinted background.
- The column is about 65ch in both fonts, the same measure as the editor, and centred in
  the preview.
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
  up with the matching block in the preview, using `data-source-line`. This works the
  same whether the preview is a separate window or docked. Preview → editor sync is v1.1.
- **Links:** clicking a link calls `backend.openExternalUrl` (http, https and mailto
  only). `#anchor` links scroll within the preview. The preview itself never navigates
  away.
- **Images (kept simple):**
  - A relative or `file:` `src` inside the document's folder or its subfolders is shown.
  - Anything else shows a small inline placeholder with the alt text. That includes
    remote URLs, paths outside the document's folder, and any image in an unsaved
    document, since it has no folder yet.
- **Raw HTML (kept simple):** it is escaped (markdown-it `html: false`), so tags appear
  as literal text.
- **Math errors:** KaTeX runs with `throwOnError: false`, so a bad expression is shown as
  source text in `--accent` with the error message as a tooltip.

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
| `PreviewBridge` (`src/previewbridge.{h,cpp}`) | `QObject`, registered on a `QWebChannel` | Properties: `markdown`, `baseUrl` (document folder as `file://…/`), `theme` (`QVariantMap`: bg, fg, accent, selection, muted, dark), `textScale`, `fontFamily` (`mono` or `quattro`), `sourceLine` (for sync). Invokables called from the page: `openLink(url)`, `ready()`. Owns the debounce timer. There is **one bridge per editor**, shared by whichever placement is active. |
| `Backend` | existing | Keeps its current API. Additions: a `previewBridge` property, `previewPlacement`, `previewVisible` and `previewFont` (all saved to `QSettings`), and feeding text, theme and scale into the bridge. The `editorTextChanged()` path already knows when content has really changed, and that is what triggers the bridge. |
| `PreviewPane.qml` | new QML file | Wraps the `WebEngineView`: transparent background, `settings.javascriptCanOpenWindows: false`, `localContentCanAccessFileUrls: true`, `localContentCanAccessRemoteUrls: false`, `onNavigationRequested` blocks everything except the initial qrc load, and `onNewWindowRequested` sends the URL to `openLink`. It knows nothing about which placement it is in. |
| `PreviewWindow.qml` | new QML file | A top-level `Window` (no `transientParent`) containing a `PreviewPane`. It owns the title, background and close-to-hide behaviour, and repeats the application-wide `Shortcut`s it needs. It is created by a `Loader` in `Main.qml`. |
| Docked layout in `Main.qml` | QML | A `SplitView` around the existing editor `Flickable`, with a `Loader` for the second `PreviewPane`. When not docked, the `SplitView` contains only the editor, and the editor subtree is unchanged. |
| `preview/index.html`, `preview.css`, `preview.js` | qrc assets | Rendering, DOM patching, theme variables, font switching, scroll sync, link and image interception. |
| `fonts/iAWriterQuattroS-*.ttf` | bundled fonts | Registered with `QFontDatabase`, as Mono is, and exposed to the page through `@font-face` pointing at `qrc:`. |
| `third_party/` | vendored JS, CSS and fonts | `markdown-it`, `markdown-it-texmath`, `katex` (min.js, min.css, woff2 fonts only), `qwebchannel.js` (copied from Qt). Exact versions pinned in `third_party/VERSIONS`, with each licence alongside. |
| `RequestInterceptor` | `QWebEngineUrlRequestInterceptor` on an off-the-record profile | Allows only `qrc:` and `file:` under the document folder; blocks everything else. This enforces principle 3. |

### 5.3 Startup and placement lifecycle
- `main.cpp` calls `QtWebEngineQuick::initialize()` **before** `QApplication` is
  constructed. Qt requires this.
- Both placements create their `PreviewPane` through a `Loader`. It becomes active only
  when `previewVisible` is true, so with the preview hidden, Chromium never starts.
- **Hiding the preview** makes its window or pane invisible but leaves the loader
  active, so showing it again is instant.
- **Switching placement** deactivates one loader and activates the other. A
  `WebEngineView` is never moved between `QQuickWindow`s, because moving it across
  windows breaks its GPU surface. The profile, and so Chromium itself, lives as long as
  the app, which makes recreating the view cheap.
- The editor window is always the primary window: `rootObjects().first()`, the
  `setParentWindow` target, and the owner of the geometry settings. The preview window
  is secondary and never closes the application.
- Targets:
  - With the preview hidden, cold start is no more than 10% slower than Omawrite's.
  - When the preview starts visible, which is the default, the first render appears
    within 600 ms of the editor window.

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
  and uses A4 or Letter from the locale. It uses the current preview font.
- **Print (`Ctrl+P`):** replaces Omawrite's `QTextDocument` path, which can't show math.
  It renders a temporary PDF, then sends it to the printer through `QPrintDialog`, using
  `QPdfDocument` (Qt PDF, already pulled in by qt6-webengine) to rasterise pages.
  If the preview is hidden or has never been loaded, print loads it offscreen first.

### 5.6 Editor-side additions
- `MarkdownHighlighter` gets a math rule. `$…$`, `$$…$$`, `\(…\)` and `\[…\]` spans are
  drawn in `themeAccent` at reduced opacity, and the delimiters are muted like the other
  Markdown syntax. Math inside fenced code is left alone.
- `smartReturn` treats an open `$$` block like an open code fence: Return inserts a
  single newline, with no list continuation.

## 6. Build, packaging and desktop integration

- `omaview.pro`: add `webenginequick webchannel pdf` to `QT`, plus the new sources and a
  second qrc (`src/preview.qrc`) for the web assets and the Quattro fonts.
- `PKGBUILD`: add `qt6-webengine` and `qt6-webchannel` to `depends`.
- `bin/build`, `bin/test` and `bin/install` stay as they are (qmake6 + make).
- The binary, desktop file, icon, `QSettings` app name and recovery directory are all
  named `omaview`. Omaview and Omawrite can be installed together without clashing.
- A qmake scope (`no_preview { … }`, enabled with `qmake6 CONFIG+=no_preview`) builds
  the editor without WebEngine, for debugging and for rebasing on upstream.

### 6.1 Omarchy and Hyprland
- On Wayland both windows share the app id `omaview`, which comes from
  `setDesktopFileName`. Window rules tell them apart by **title**: the preview title
  always starts with `Preview — `.
- Out of the box nothing is configured: Hyprland tiles the preview next to the editor,
  and it keeps Omarchy's gaps, borders and opacity.
- `docs/omarchy.md` documents optional snippets for `~/.config/hypr/`. They are written
  in the syntax of the current Hyprland release (check it with the `omarchy` skill),
  never applied automatically:
  - no initial focus for the preview window;
  - opening the preview on the same workspace to the right;
  - an Omarchy-style launcher entry.
- Test with Omarchy's default dwindle layout and with master layout.

## 7. Testing

- **Existing suite:** all 12 inherited `QtTest` cases must keep passing (`bin/test`,
  offscreen).
- **Bridge and backend unit tests (C++):**
  - debounce coalescing;
  - the theme map's contents after `colors.toml` changes;
  - `baseUrl` for saved and unsaved documents;
  - link filtering (reject `javascript:` and `file:` outside the document folder);
  - defaults and saving for `previewPlacement`, `previewVisible` and `previewFont`.
- **Render tests:** a `tests/preview/` QtTest target loads `index.html` in an offscreen
  `QQuickWebEngineView` from a fixture corpus (`tests/fixtures/*.md`). It asserts, using
  `runJavaScript`:
  - the number of `.katex` and `.katex-display` nodes;
  - no `.katex-error` in valid fixtures, and error nodes in invalid ones;
  - currency like `$5 and $10` not treated as math;
  - math inside code spans or fences staying literal;
  - raw HTML escaped;
  - remote and out-of-folder images replaced by placeholders;
  - the computed `font-family` following `fontFamily`.
- **Placement test:** switching placement twice keeps the rendered content and the
  `sourceLine` scroll anchor, and leaves exactly one live `WebEngineView`.
- **Performance test:** the 2,000-line fixture must meet the budget in 5.4. It is run
  manually and logged, not gated in CI.
- **Manual checklist (Omarchy):**
  - the preview tiles beside the editor on first launch, and focus stays in the editor;
  - Super+W on the preview only hides it;
  - closing the editor closes the preview;
  - fractional scaling at 1.25 and 1.5;
  - changing theme with `omarchy theme set …` and text size with
    `omarchy display text size …` updates both windows live;
  - dark and light toggle;
  - the portal dialogs for PDF export.

## 8. Milestones

| # | Deliverable | Acceptance |
|---|---|---|
| M0 | Fork and rename *(done)* | Builds as `omaview`, 12/12 tests pass, upstream remote kept |
| M1 | Pop-out preview window | The preview opens as a separate tiled window by default and `Ctrl+E` shows and hides it, as does the footer preview button. Markdown and math render from qrc with no network. Theme and scale apply live. Chromium loads only when the preview is shown. Each update re-renders the whole of `#content` and keeps the scroll position; block-keyed patching comes in M2. The `no_preview` qmake scope builds and passes the inherited tests. |
| M2 | Fonts, incremental render and scroll sync | Mono/Quattro switching (`Ctrl+Shift+T`). Block-keyed DOM patching. The performance budget is met. Editor→preview sync works. Links, images and escaped HTML behave as in 4.4. |
| M3 | Docked placement | `Ctrl+Shift+E` switches placement. `SplitView` with a saved ratio. The narrow-window rule. The placement test passes. |
| M4 | Editor math support | Highlighter rule, `Ctrl+M` / `Ctrl+Shift+M`, and `$$`-aware smart return, each with tests |
| M5 | PDF export and print | `Ctrl+Shift+P` and `Ctrl+P` produce output with the math rendered |
| M6 | Packaging and Omarchy docs | PKGBUILD dependencies, desktop file, icon variant, README, `docs/omarchy.md`, `bin/install` works |

## 9. Decisions log

| Date | Decision |
|---|---|
| 2026-09-21 | The preview defaults to its own top-level window, so Hyprland tiles it on Omarchy. Docked split is optional. |
| 2026-09-21 | The preview font matches the editor (iA Writer Mono S). It can be switched to the proportional iA Writer Quattro S. |
| 2026-09-21 | Raw HTML is escaped and remote images are blocked, with no opt-ins in v1. |
| 2026-09-21 | Parse Markdown with markdown-it and render math with KaTeX, rather than md4c, to get source-line mapping. |
| 2026-09-21 | Use `markdown-it-texmath` rather than `@vscode/markdown-it-katex`, because only texmath supports the `\(…\)` and `\[…\]` delimiters as well as `$`. |
| 2026-09-21 | M1 ships the footer preview button and the `no_preview` qmake scope, and renders by replacing the whole of `#content`. Block patching stays in M2. |

## 10. Open questions

1. **KaTeX macros:** read a `macros:` block from YAML front matter, or a
   `~/.config/omaview/macros.tex` file?
2. **Restoring the preview's workspace:** if the user moves the preview to another
   workspace or monitor, should Omaview try to reopen it there next time? Hyprland has
   no portable way to do this, so the current answer is no.
