# Omalorem — Specification

Status: Draft v1.1 · 2026-09-25
Upstream: [omacom-io/omawrite](https://github.com/omacom-io/omawrite) (MIT), forked at `8f98892` (Omawrite 0.5.0)

## 1. Purpose

*The name.* Omalorem was released as Omaview 0.1.0 (tag `omaview-v0.1.0`) and renamed
straight after. The name comes from *lorem ipsum*, the standard placeholder text, because
the project is meant as a small extension to Omawrite rather than a new app of its own.

Omalorem is Omawrite plus a live preview. You write Markdown in Omawrite's editor, and a
separate preview window shows it rendered, with LaTeX math typeset by KaTeX. It is meant
for notes, papers and technical writing where you need to check that the equations are
right while you write.

Omalorem keeps Omawrite's character: no toolbars, no sidebars, no settings dialog, a
narrow typographic column, and theme and text size taken from the desktop. Omalorem is
designed for **Omarchy and other tiling window managers only**. The preview is its own
top-level window, and Hyprland tiles it beside the editor, so the window manager handles
the layout rather than the app. Omalorem deliberately has no layout of its own, such as a
docked split view, because supporting other desktops would make the project more complex
than a small extension of Omawrite should be. A docked placement stays on the list of
future features (§11), in case other users want it.

## 2. Principles

1. **The editor is untouched.** Everything Omawrite does (smart return, link paste,
   recovery, external-change detection, search and replace, the wheel physics) keeps
   working exactly as it does upstream. The preview is added beside it. The fork should
   stay easy to rebase onto upstream Omawrite, and the preview easy to port to another
   editor. So changes to upstream files are **additive only**: new blocks, properties,
   signals and files. No existing upstream line is modified, with two exceptions: the
   `Ctrl+?` reference text (§4.2), and the lines that carry the project's name, which
   were renamed at the fork (Omawrite → Omaview) and again at the rename (Omaview →
   Omalorem). Preview behaviour lives in the preview's own files and
   reaches the editor through the hooks those additive blocks expose.
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
- A live preview in its **own top-level window**, which the tiling window manager places
  beside the editor. `Ctrl+E` shows or hides it.
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
- **Non-tiling desktops, and any layout managed by the app.** There is no docked or
  split-view placement, and no saved window geometry for the preview. The preview window
  still opens on a floating desktop, but arranging it is left to the user. The docked
  placement is a future feature (§11).
- Windows and macOS. WebView2 is not used. The target is Linux on Wayland with Hyprland,
  as Omarchy ships it. Other tiling compositors, such as Sway, and tiling X11 window
  managers, such as i3, are best effort.

## 4. User experience

### 4.1 The preview window

The preview is always a separate window. Whether it is showing is saved as
`preview/visible`, which defaults to `true`. (Backend also stores `preview/placement`,
added in M1. Only the value `window` is used; the setting stays dormant for the future
docked placement in §11.)

This is the view on Omarchy, with Hyprland tiling the two windows side by side:

```
┌──────────── notes.md - Omalorem ───────────┐┌──────── Preview — notes.md - Omalorem ─────┐
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
  `Preview — <fileName> - Omalorem`, following the editor's title as it changes. Its
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
- **Geometry.** Omalorem does not save the preview window's size or position. Hyprland
  decides it when tiling. If the user floats the window, the compositor takes over.
- **Shortcuts in the preview.** Every `Qt.ApplicationShortcut` defined in `Main.qml`
  (save, open, print, find, `Ctrl+E`, and so on) already fires while the preview window
  has focus, so `PreviewWindow` does **not** repeat them: a second `ApplicationShortcut`
  with the same sequence makes Qt treat it as ambiguous, and neither fires. Shortcuts
  that edit text (`Ctrl+B`, `Ctrl+Z`, …) do nothing there. Find, when started from the
  preview, focuses the editor window; `PreviewWindow` does this by reacting to the
  editor's `searchOpen`, not by changing the editor's find handlers. One known limit
  follows from that: if find is already open, `Ctrl+F` from the preview opens nothing new,
  `searchOpen` doesn't change, and focus stays in the preview. This is accepted, to keep
  the editor's code untouched. F11 in the preview
  fullscreens the preview, handled inside the preview's own files. On Omarchy,
  Hyprland's own fullscreen (`Super+F`) works on either window.

The footer gets one extra `FooterIconButton` (`iconName: "preview"`) that shows or hides
the preview. The word count stays at the bottom right of the editor window.

### 4.2 Shortcuts (added to Omawrite's)

| Shortcut | Action |
|---|---|
| `Ctrl+E` | Show or hide the preview window |
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
  proportional version of the same family. It has the same OFL licence, and Omalorem
  bundles Regular, Italic, Bold and BoldItalic as woff2, in the preview plugin only. The
  choice is saved in `preview/font` (`mono` by default, or `quattro`). It applies
  straight away and doesn't touch the editor font. Code spans and fenced code always
  stay in Mono.
- Headings use weight and size only, never colour. Code uses the same font on a faint
  tinted background.
- The column is about 65ch in both fonts, the same measure as the editor, and centred in
  the preview. `#content` itself stays in Mono and only its blocks take the prose font,
  so `65ch` is the same width in both.
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
  - The editor publishes a fractional `sourceLine`: the line at the top of its view, plus
    the fraction of that line's height scrolled past. It probes at the view's top, which
    is the editor's top margin above where the text begins; the page puts the matching
    point its own top margin below its view's top, so both are at 0 at the start.
  - The page interpolates inside a block by its source lines (`data-source-line` to
    `data-source-line-end`) and across the blank lines between blocks.
  - The preview **follows** the editor until the reader scrolls the preview (wheel,
    keys, pointer) or follows an `#anchor`, and follows again as soon as the editor
    scrolls. While following, renders, resizes and late-loading fonts re-apply the
    sync; otherwise the scroll position stays where the reader put it.
- **Links:** clicking a link calls `backend.openExternalUrl` (http, https and mailto
  only). `#anchor` links scroll within the preview: headings get GitHub-style ids (text
  lower-cased, punctuation dropped, spaces as hyphens, `-1`, `-2` on repeats). The
  preview itself never navigates away.
- **Images (kept simple):**
  - A relative or `file:` `src` inside the document's folder or its subfolders is shown.
  - Anything else shows a small inline placeholder with the alt text. That includes
    remote URLs, paths outside the document's folder, and any image in an unsaved
    document, since it has no folder yet.
  - The placeholder's tooltip says why the image isn't shown.
  - Chromium refuses `file:` URLs to a page loaded from `qrc:`, even when access to local
    files is enabled. So document images are served through a custom URL scheme,
    `omalorem-doc:/<path in folder>`, and the page rewrites relative and in-folder `file:`
    `src`s to it. The plugin registers the scheme as it loads (before any profile
    exists) as a secure, path-syntax scheme. It is not `LocalScheme`, because Chromium
    doesn't count `qrc:` as local and would lock the page out. Its
    `QWebEngineUrlSchemeHandler` is installed on the preview profile only. It serves
    `GET` requests for **image files** (by MIME type) whose path, with symlinks
    resolved, is inside the document's folder, and refuses everything else.
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
| `PreviewBridge` (`src/previewbridge.{h,cpp}`) | `QObject`, registered on a `QWebChannel` | Properties: `markdown`, `baseUrl` (document folder as `file://…/`), `theme` (`QVariantMap`: bg, fg, accent, selection, muted, dark), `textScale`, `fontFamily` (`mono` or `quattro`), `sourceLine` (a `qreal`: line plus fraction, for sync). Invokables called from the page: `openLink(url)`, `ready()`. Owns the debounce timer. There is **one bridge per editor**, which outlives any view of it. |
| `Backend` | existing | Keeps its current API. Additions: a `previewBridge` property, `previewVisible` and `previewFont` (saved to `QSettings`), a dormant `previewPlacement` (only `window` is used; see §11), `previewLineAt(position)` for scroll sync, and feeding text, theme, scale and font into the bridge. The `editorTextChanged()` path already knows when content has really changed, and that is what triggers the bridge. |
| `PreviewPane.qml` (`src/previewplugin/`) | plugin QML file | Wraps the `WebEngineView`: transparent background, `settings.javascriptCanOpenWindows: false`, `localContentCanAccessFileUrls: false` (images come through `omalorem-doc:`), `localContentCanAccessRemoteUrls: false`, a `WebChannel` with `propertyUpdateInterval: 16` so scroll sync keeps up with the editor, `onNavigationRequested` blocks everything except the initial qrc load, and `onNewWindowRequested` sends the URL to `openLink`. It knows nothing about the window it sits in, so it can be reused, for example by print (M5). |
| `PreviewWindow.qml` (`src/previewplugin/`) | plugin QML file | A top-level `Window` (no `transientParent`) containing a `PreviewPane`. It owns the title, background and close-to-hide behaviour, and moves focus to the editor when find opens. It does not repeat the application-wide `Shortcut`s (see §4.1). It is created by a `Loader` in `Main.qml`, through `src/PreviewHost.qml`, whose `import Omalorem.Preview` is what loads the plugin. |
| `Omalorem.Preview` plugin (`src/previewplugin/`) | QML plugin module (`previewplugin.pro`, `qmldir`, plugin class, qrc) | Holds everything that depends on QtWebEngine. Built by an extra make target in `omalorem.pro` into `build/Omalorem/Preview/`. `main.cpp` adds its import path and turns the preview on only if its `qmldir` exists; without it, Omalorem runs as plain Omawrite. |
| `src/preview/index.html`, `preview.css`, `preview.js` | qrc assets in the plugin | Rendering, DOM patching, theme variables, font switching, scroll sync, link and image interception. |
| `fonts/iAWriterQuattroS-*.woff2` | bundled fonts | Compiled into the preview plugin's qrc only and exposed to the page through `@font-face`. They are not registered with `QFontDatabase`, because the editor never uses them. Provenance is in `third_party/VERSIONS`. |
| `third_party/` | vendored JS, CSS and fonts | `markdown-it`, `markdown-it-texmath`, `katex` (min.js, min.css, woff2 fonts only), `qwebchannel.js` (copied from Qt). Exact versions pinned in `third_party/VERSIONS`, with each licence alongside. |
| `PreviewSandbox` and its interceptor (`src/previewplugin/previewsandbox.{h,cpp}`) | QML singleton, `QWebEngineUrlRequestInterceptor` and `QWebEngineUrlSchemeHandler`, in the plugin | `PreviewSandbox.protect()` attaches the interceptor and the `omalorem-doc:` handler to the off-the-record profile that `PreviewPane` builds from a `WebEngineProfilePrototype` with no storage name. The URL policy itself is header-only (`src/previewpolicy.h`), so the plugin needs no symbols from the executable and the WebEngine-free tests can cover it. |
| Interceptor policy (`src/previewpolicy.h`) | header-only C++ | Allows only `qrc:` and `omalorem-doc:` URLs that map inside the document's folder (`previewDocumentPath`), and blocks everything else, `file:` included. This enforces principle 3. |

### 5.3 Startup and lifecycle
- **The `omalorem` binary does not link QtWebEngine.** Just linking `libQt6WebEngineCore`
  costs about 90 ms on every launch, which breaks the startup target below. The preview
  (`PreviewSandbox`, `PreviewPane`, `PreviewWindow` and the request interceptor) is built
  as a QML plugin module that the `Loader` imports only when the preview is first shown,
  so the WebEngine libraries load then. `main.cpp` does not call
  `QtWebEngineQuick::initialize()`, because calling it would link WebEngine. It sets
  `Qt::AA_ShareOpenGLContexts` before `QApplication` is constructed instead, which is
  the requirement `initialize()` exists to meet. `PreviewBridge` stays in the main
  binary, because it has no WebEngine dependency.
- The preview window, and its `PreviewPane`, are created through a `Loader`. It becomes
  active only when `previewVisible` is true, so with the preview hidden, Chromium never
  starts.
- **Hiding the preview** makes its window invisible but leaves the loader active, so
  showing it again is instant.
- **Profile lifetime.** Since M1.1 the profile is built from a `WebEngineProfilePrototype`
  inside `PreviewPane`, so it lives only as long as its pane. The view itself is created by
  a `Loader` after the pane completes, because the prototype returns no profile before
  then, and `setProfile(nullptr)` crashes. That is fine while there is one pane for the
  life of the app. Anything that creates a second pane, such as offscreen printing (M5)
  or a docked placement (§11), must decide whether the profile moves out of the pane, for
  example into `PreviewSandbox`. A `WebEngineView` is never moved between
  `QQuickWindow`s, because moving it across windows breaks its GPU surface.
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

As built in M2:
- The key is the block's rendered HTML, not its source. It is exact, and it covers things
  a source hash misses, such as a reference definition elsewhere changing a link.
- Source lines are left out of the HTML and set on the nodes after patching, so lines
  added above a block don't re-create it.
- Every new block is parsed in one `<template>`, split by marker comments. Raw HTML is
  escaped, so a document can't forge one.
- Stale nodes are removed before new ones are inserted, so blocks that stay are never
  moved.
- KaTeX output is cached by `(displayMode, tex)`. Entries unused for a whole render are
  dropped.
- Measured offscreen with no GPU: a median of 9.5 ms per update, including layout. The
  first update after load pays once more, when KaTeX's web fonts arrive.

### 5.5 Printing and PDF
- **Export PDF:** `WebEngineView.printToPdf(path, pageSize, orientation)`, with the path
  chosen through the existing portal `FileDialog` (Save mode, `*.pdf`). A print
  stylesheet (`@media print`) forces the light palette, removes the column centring,
  and uses A4 or Letter from the locale. It uses the current preview font.
- **Print (`Ctrl+P`):** replaces Omawrite's `QTextDocument` path, which can't show math.
  It renders a temporary PDF, then sends it to the printer through `QPrintDialog`, using
  `QPdfDocument` (Qt PDF, already pulled in by qt6-webengine) to rasterise pages.
  If the preview is hidden or has never been loaded, print loads it offscreen first.
- These need the preview. In a `no_preview` build `Ctrl+P` is Omawrite's print, and
  `Ctrl+Shift+P` does nothing.

As built in M5:
- **Routing `Ctrl+P`.** Omawrite's `Ctrl+P` shortcut is left alone, because a second
  one with the same sequence would make Qt treat both as ambiguous. Instead, lines added
  at the top of `Backend::printDocument()` emit `previewPrintRequested()` when the preview
  is available, and return. Otherwise Omawrite's print runs as before.
- **One path for both outputs.**
  - `Main.qml`'s preview block owns `Ctrl+Shift+P` and the PDF `FileDialog`. It
    suggests `<document name>.pdf` beside the document, or where Save would suggest.
  - It hands the request to `PreviewWindow.produce()`. If the preview window has never
    been loaded, it loads it first; with the preview hidden, the window stays hidden and
    stays loaded afterwards.
  - Printing from the hidden window's own page means there is only ever one pane, so
    the profile-lifetime question (§5.3) doesn't come up for printing.
- **Waiting for the page.** Output flushes the editor's text past the debounce
  (`previewFlushMarkdown`). It then waits until the page reports, through
  `PreviewBridge::rendered(revision)`, that it shows the current `markdownRevision` with
  its fonts and images loaded (at most 3 s for an image). A 30 s timeout reports a page
  that never settles.
- **Paper.** Letter where the locale uses the US measurement system, otherwise A4, in
  portrait.
- **Margins.** Qt's QML `printToPdf` gives Chromium zero margins, and Chromium then
  ignores an `@page` margin. So the print stylesheet pads the body by 20 mm with
  `box-decoration-break: clone`, which repeats the padding on every page.
- **Print stylesheet.** The light palette (with `!important`, because the theme is
  set inline), 11 pt text in the current preview font, and links as plain text. Headings
  are kept with what follows, and code, tables, formulas and list items aren't split
  across pages. Long code lines wrap.
- **Wide formulas.** `preparePrint(pageWidthMm)` scales any display formula wider than
  the printed column to fit, through a `--print-zoom` property that only print uses.
  This is the simple version; see §11.2.
- **Print, through the desktop's print portal.**
  - When `org.freedesktop.portal.Print` is available (on Omarchy, through
    xdg-desktop-portal-gtk), `Ctrl+P` calls `PreparePrint` first, so the GTK print dialog
    opens at once.
  - Once the dialog is accepted, the page is rendered on the paper and orientation
    chosen there (`paperFromPageSetup`, which maps the size to a `QPageSize` id).
  - The PDF itself goes to `Print` with the returned token, so the output stays vector.
  - Cancelling renders nothing.
  - **Why the portal:** Qt's `QPrintDialog` asks CUPS for its printers several times
    before it opens, and on a system with Avahi each query waited a full second. That
    measured about 10 s with no printers configured. Omawrite's own `Ctrl+P` pays the
    same cost.
- **Print, fallback without the portal.** `PreviewSandbox.printPdf()` shows `QPrintDialog`
  after rendering, owned by the editor window and limited to the PDF's pages. It draws
  each chosen page as an image at up to 300 dpi, fitted to the sheet.
- **Tests.** `printTestTarget` sends the fallback's output to a PDF file without the
  dialog, and `portalService` points the portal path at a mock. The test binary sets
  `OMALOREM_PRINT_PORTAL` to a service that doesn't exist, so no test can open a real
  dialog.

### 5.6 Editor-side additions
- `MarkdownHighlighter` gets a math rule. `$…$`, `$$…$$`, `\(…\)` and `\[…\]` spans are
  drawn in `themeAccent` at reduced opacity, and the delimiters are muted like the other
  Markdown syntax. Math inside fenced code is left alone.
- `smartReturn` treats an open `$$` block like an open code fence: Return inserts a
  single newline, with no list continuation.
- These work in every build, `no_preview` included; none of them needs WebEngine.

As built in M4:
- **One scanner, two users.** `MarkdownHighlighter::mathSpans(line, previousState)`
  finds the spans in one line. It follows the preview's rules (texmath plus the tightened
  inline `$` rule in `preview.js`):
  - no space just inside `$…$`;
  - no digit just before an opening `$` or just after a closing one;
  - `\$` and code spans are never math.
- **Block state.** Display math and fences carry a block state from line to line
  (`MathState`). Upstream never sets one, so every state belongs to this rule.
- **Emphasis markers inside math.** Omawrite hides the markers of `*`/`_` emphasis and
  the caret skips them. Inside math those characters are part of the formula, so any
  emphasis item with a marker inside a formula is dropped. This applies both in
  `highlightInline` and in `Backend::hiddenRangesAt`. Without it, `$x_1 + y_2$` would
  show as `x1 + y2`.
- **Formats.** Math is `themeAccent` at 80% opacity, and the delimiters use the muted
  marker colour.
- **Shortcuts.**
  - The edits are pure functions in `src/EditorMath.js`: `inlineMath` and
    `displayMath`.
  - `Ctrl+M` leaves whitespace at either end of the selection outside the dollars.
  - `Ctrl+Shift+M` puts a blank line on each side of the block when it's missing,
    because a `$$` block can't interrupt a paragraph in the preview's parser.
  - `Backend::replaceTextAsOneEdit` applies each edit in one `QTextCursor` edit block,
    so a single undo reverts it. Omawrite's `Ctrl+B` still takes two undos, because
    `EditorMutations` removes and then inserts.
- **Smart return** asks `Backend::displayMathOpenAt(position)`, which scans the line up
  to the caret from the previous block's state.
- **Known limits:**
  - An inline `$…$` that wraps onto a second source line is not highlighted, though the
    preview renders it.
  - Indented (four-space) code blocks are not excluded, only fences.
  - `Ctrl+Shift+M` on a whitespace-only line replaces that line's indentation.

## 6. Build, packaging and desktop integration

- `omalorem.pro`: the executable does **not** add WebEngine to `QT` (§5.3). New sources
  are added on fresh `SOURCES +=` lines, so upstream lines stay untouched (principle 1).
  An extra make target builds the plugin (`src/previewplugin/previewplugin.pro`), which
  is where `webenginequick webchannel` live, plus `pdf` in M5. The plugin's qrc holds the
  web assets, and the Quattro fonts from M2.
- `PKGBUILD` (M6):
  - add `qt6-webengine` and `qt6-webchannel` to `depends`;
  - in `package()`, also run `make -C build/preview-plugin install INSTALL_ROOT="$pkgdir"`.
    That installs `/usr/lib/omalorem/qml/Omalorem/Preview/{libomalorempreviewplugin.so,qmldir}`;
    qmake's `PREVIEW_INSTALL_DIR` overrides the path;
  - keep the existing `install -Dm755 build/omalorem …/usr/bin/omalorem`. `main.cpp` finds
    the plugin at `<bindir>/../lib/omalorem/qml` (and first at `build/` for development).
- `bin/build`, `bin/test` and `bin/install` keep their upstream lines (qmake6 + make).
  `bin/test` gains appended lines that build and run the `tests/preview/` target.
  `bin/build-no-preview` is new, and builds the `no_preview` variant into
  `build-no-preview/`.
- The binary, desktop file, icon, `QSettings` app name and recovery directory are all
  named `omalorem`. Omalorem and Omawrite can be installed together without clashing.
- A qmake scope (`no_preview { … }`, enabled with `qmake6 CONFIG+=no_preview`) builds
  the editor without WebEngine or the plugin, for debugging and for rebasing on upstream.

### 6.1 Omarchy and Hyprland
- On Wayland both windows share the app id `omalorem`, which comes from
  `setDesktopFileName`. Window rules tell them apart by **title**: the preview title
  always starts with `Preview — `.
- Out of the box nothing is configured: Hyprland tiles the preview next to the editor,
  and it keeps Omarchy's gaps, borders and opacity.
- `docs/omarchy.md` documents optional snippets for `~/.config/hypr/`, never applied
  automatically. As written in M6, for Omarchy 4 and Hyprland 0.56:
  - Omarchy configures Hyprland in **Lua**, so the snippets use its `o.window(match,
    rules)` helper rather than `windowrule` lines.
  - The snippets cover `no_initial_focus` for the preview, floating it at a fixed size,
    keeping it out of screen shares, and floating portal dialogs for non-Omarchy
    desktops.
  - Two things need no configuration on Omarchy: it already floats and centres
    `xdg-desktop-portal-gtk` windows (so the print and save dialogs behave), and its
    `misc.focus_on_activate = true` is what lets the editor keep the keyboard when the
    preview opens.
  - It also covers the launcher entry, a keybinding, themes and text size, and what to
    do if the preview misbehaves.
- Test with Omarchy's default dwindle layout and with master layout.

## 7. Testing

- **Existing suite:** all 12 inherited `QtTest` cases must keep passing (`bin/test`,
  offscreen).
- **Bridge and backend unit tests (C++):**
  - debounce coalescing;
  - the theme map's contents after `colors.toml` changes;
  - `baseUrl` for saved and unsaved documents;
  - link filtering (reject `javascript:` and `file:` outside the document folder);
  - defaults and saving for `previewVisible` and `previewFont`, and for the dormant
    `previewPlacement`.
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
| M0 | Fork and rename *(done)* | Built as `omaview`, 12/12 tests pass, upstream remote kept. Renamed to `omalorem` after the 0.1.0 release |
| M1 | Pop-out preview window *(done)* | The preview opens as a separate tiled window by default and `Ctrl+E` shows and hides it, as does the footer preview button. Markdown and math render from qrc with no network. Theme and scale apply live. Chromium loads only when the preview is shown. Each update re-renders the whole of `#content` and keeps the scroll position; block-keyed patching comes in M2. The `no_preview` qmake scope builds and passes the inherited tests. |
| M1.1 | Lazy WebEngine and an additive-only upstream diff *(done)* | The binary does not link QtWebEngine (`ldd`), and the preview loads as a QML plugin on first show. With the preview hidden, cold start is within 10% of the `no_preview` build. Upstream files differ from the fork point only by added lines (plus the `Ctrl+?` text). The spec in §4.1 and §5.2 matches how shortcuts actually behave. All tests pass. |
| M2 | Fonts, incremental render and scroll sync *(done)* | Mono/Quattro switching (`Ctrl+Shift+T`). Block-keyed DOM patching. The performance budget is met. Editor→preview sync works. Links, images (served through the document-folder scheme) and escaped HTML behave as in 4.4. |
| M3 | ~~Docked placement~~ *(deferred)* | Moved to the future features in §11. Omalorem targets Omarchy and tiling window managers only. The milestone numbers after it are kept. |
| M4 | Editor math support *(done)* | Highlighter rule, `Ctrl+M` / `Ctrl+Shift+M`, and `$$`-aware smart return, each with tests. Working in `no_preview` builds too |
| M5 | PDF export and print *(done)* | `Ctrl+Shift+P` and `Ctrl+P` produce output with the math rendered |
| M6 | Packaging and Omarchy docs *(done, bar the release)* | PKGBUILD dependencies, desktop file, icon variant, README, `docs/omarchy.md`, `bin/install` works |

## 9. Decisions log

| Date | Decision |
|---|---|
| 2026-09-21 | The preview defaults to its own top-level window, so Hyprland tiles it on Omarchy. Docked split is optional. *(Superseded below: there is no docked split in v1.)* |
| 2026-09-21 | The preview font matches the editor (iA Writer Mono S). It can be switched to the proportional iA Writer Quattro S. |
| 2026-09-21 | Raw HTML is escaped and remote images are blocked, with no opt-ins in v1. |
| 2026-09-21 | Parse Markdown with markdown-it and render math with KaTeX, rather than md4c, to get source-line mapping. |
| 2026-09-21 | Use `markdown-it-texmath` rather than `@vscode/markdown-it-katex`, because only texmath supports the `\(…\)` and `\[…\]` delimiters as well as `$`. |
| 2026-09-21 | M1 ships the footer preview button and the `no_preview` qmake scope, and renders by replacing the whole of `#content`. Block patching stays in M2. |
| 2026-09-21 | M1.1: the binary no longer links QtWebEngine; the preview becomes a QML plugin loaded on first show, replacing the early `QtWebEngineQuick::initialize()` call (a measured +26% cold start with the preview hidden). |
| 2026-09-21 | Changes to upstream files are additive only, so the fork rebases cleanly and the preview can be ported. |
| 2026-09-21 | `PreviewWindow` does not repeat application shortcuts, because Qt treats duplicates as ambiguous. |
| 2026-09-21 | Local images go through a custom document-folder URL scheme in M2, because Chromium blocks `file:` from `qrc:` pages. |
| 2026-09-21 | `Ctrl+F` from the preview while find is already open leaves focus in the preview. Accepted, to keep the editor's find handlers untouched. |
| 2026-09-21 | The preview's QtWebEngine code lives in the `Omalorem.Preview` plugin under `src/previewplugin/`, and the plugin installs to `/usr/lib/omalorem/qml`. |
| 2026-09-21 | M2: Quattro S ships as woff2 in the plugin only, not registered with `QFontDatabase`. |
| 2026-09-21 | M2: the resource policy drops `file:`; document images come only through `omalorem-doc:`, which serves image files inside the folder with symlinks resolved. |
| 2026-09-21 | Released as Omaview 0.1.0 (tag `omaview-v0.1.0`), then renamed Omalorem, after *lorem ipsum* placeholder text: a small extension to Omawrite. Binary, desktop file, icon, settings, recovery directory, QML module (`Omalorem.Preview`), plugin install path (`/usr/lib/omalorem/qml`), logging categories and the `omalorem-doc:` scheme all follow the name. |
| 2026-09-21 | M2: `sourceLine` is fractional, and the preview follows the editor until the reader scrolls the preview. |
| 2026-09-21 | M4: the editor math features work in every build, `no_preview` included. Emphasis markers inside math are neither hidden nor skipped by the caret. `Ctrl+M` and `Ctrl+Shift+M` are one undo step each. |
| 2026-09-21 | M5: `Ctrl+P` reaches the preview through lines added to `Backend::printDocument()`. With the preview hidden, output comes from the hidden preview window, never a second pane. Page margins come from cloned body padding, because Qt's `printToPdf` has none. Wide formulas are scaled to fit for now (§11.2). Output stays simple: no headers, page numbers or link URLs (§11.3). |
| 2026-09-25 | M6: `pkgbuild/` counts as the fork's own directory, like `README.md` and `docs/`. A renamed fork's packaging carries its own identity (icon, desktop entry, package name), so it can't stay additive-only against Omawrite's. |
| 2026-09-25 | M6: the first release under the new name is 0.2.0, tagged `omalorem-v0.2.0`. Upstream's `v*` tags are in this repository, so Omalorem's carry the name. |
| 2026-09-21 | M5: print goes through xdg-desktop-portal's Print interface when it's there. Its dialog opens at once, and it prints the PDF as vector output. Qt's `QPrintDialog` is the fallback, because it waited about 10 s on CUPS printer discovery. |
| 2026-09-21 | Omalorem is for Omarchy and other tiling window managers only. The docked placement (old M3) moves to the future features (§11), for other users who may want it, because supporting non-tiling desktops adds complexity the project shouldn't carry. `Ctrl+Shift+E` stays unassigned. |

## 10. Open questions

1. **KaTeX macros:** read a `macros:` block from YAML front matter, or a
   `~/.config/omalorem/macros.tex` file?
2. **Restoring the preview's workspace:** if the user moves the preview to another
   workspace or monitor, should Omalorem try to reopen it there next time? Hyprland has
   no portable way to do this, so the current answer is no.
3. **First-render target (§5.3, 600 ms).** Since M1.1 the WebEngine libraries load on
   first show, and offscreen (no GPU) the first render comes 732 ms after the editor's
   first frame. It needs measuring on Hyprland with the GPU. If it still misses, relax
   the target rather than warm WebEngine up on a background thread, which isn't safe.

## 11. Future features

These are wanted but not planned. They go into a milestone only if users ask for them,
and only if they don't add complexity to the Omarchy experience, which comes first.

### 11.1 Docked placement

A docked split view inside the editor window, for non-tiling desktops or for anyone who
prefers a single window. It was planned as M3 and then deferred (§9). The design is kept
here so that it can be picked up without starting again.

**Settings and shortcut.** Two placements, saved as `preview/placement` with the values
`window` (the default and today's behaviour) and `docked`. `Ctrl+Shift+E` switches
between them, and `Ctrl+E` shows or hides the preview in either one.

**Layout.** For desktops without tiling, or for a single-window workflow:

```
┌─────────────────────────────── Omalorem ───────────────────────────────┐
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
new one in the other placement. It then re-renders from the bridge's current
state and restores the scroll position from `sourceLine`.

**Components.** A new additive block in `Main.qml` puts a `SplitView` around the existing
editor `Flickable`, with a `Loader` for a second `PreviewPane`. When not docked, the
`SplitView` contains only the editor, and the editor subtree is unchanged.

**Lifecycle.**
- **Switching placement** deactivates one loader and activates the other. A
  `WebEngineView` is never moved between `QQuickWindow`s, because moving it across
  windows breaks its GPU surface. Once loaded, the WebEngine libraries and Chromium's
  processes stay for the life of the app, which makes recreating the view cheap.
- The profile question in §5.3 applies here: the profile would probably move out of the
  pane, for example into `PreviewSandbox`, so that switching placement reuses it.

**Tests.**
- Saving the split ratio and clamping it, and the narrow-window rule.
- **Placement test:** switching placement twice keeps the rendered content and the
  `sourceLine` scroll anchor, and leaves exactly one live `WebEngineView`.

**What is already in place.**
- Backend's `previewPlacement` property and its `preview/placement` setting, with a test.
- A `PreviewPane` that doesn't depend on its window.
- A bridge that holds the scroll position (`sourceLine`), so a new view lines up as soon
  as it renders.
- A `protect()` that skips a profile which already has the `omalorem-doc:` handler.

**Before building it:** settle the profile-lifetime question in §5.3.

### 11.2 Wide formulas in print

For now, a display formula wider than the printed column is scaled down until it fits
(§5.5). That keeps it whole, but a very long formula can end up too small to read. Proper
handling could:
- break long formulas at relation or operator signs, which KaTeX can do for inline math
  but not for display math;
- let the writer mark a formula for a landscape page or a smaller size;
- set a lower limit on the scale, below which the formula wraps or moves to its own
  landscape page.

### 11.3 Page options for PDF and print

M5 keeps output simple: fixed 20 mm margins, portrait, A4 or Letter from the locale, no
header or footer, no page numbers, and links as plain text. Options that could come
later, set in `QSettings` and changed by shortcut, since there is no settings UI:
- page numbers, and a header or footer with the document's title;
- link targets printed after the link text, or as footnotes;
- margin width, page size and orientation;
- a title page, or front matter taken from YAML.
