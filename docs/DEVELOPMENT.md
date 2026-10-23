# Omalorem — Development status

Last updated: 2026-10-23 · Branch: `main`, pushed to `origin` (github.com/shakes76/omalorem). Upstream Omawrite is the `upstream` remote. Releases: `omalorem-v0.4.0` (current), then `omalorem-v0.3.0`, `omalorem-v0.2.0` and `omaview-v0.1.0` (the last commit under the old name). Release tags carry the name, because upstream's `v…` tags are in this repository

This is the working log for developers and coding agents. It records where the project
stands, how the code is laid out, the rules every change must follow, and what was done
and learned in each milestone. **Read `docs/SPEC.md` first.** The spec says what Omalorem
should be; this file says where the code is now. When they disagree, the spec wins, and
the gap should be fixed or raised.

## 1. Current state at a glance

| Milestone | Status | Notes |
|---|---|---|
| M0 Fork and rename | Done | Upstream Omawrite `8f98892`, fork commit `eb6bd1a` |
| Release 0.1.0 and rename to Omalorem | Done | Tag `omaview-v0.1.0` at `344343b`, then the rename commit (§8) |
| M1 Pop-out preview window | Done | Commits `b908f24`…`96906ed` |
| M1.1 Lazy WebEngine, additive-only upstream diff | Done | Commits `fe28c62`, `22db1c8` |
| Fix: black preview after a workspace switch | Done, confirmed on Hyprland | Commit `c625da3` |
| M2 Fonts, incremental render, scroll sync, images | Done | Commits `686f38d`…`2acb47c` |
| M3 Docked placement | Deferred | Moved to SPEC §11, future features. Omalorem targets Omarchy and tiling window managers only |
| M4 Editor math support | Done | Commits `71ce263`, `e0ab290`. See §8 |
| M5 PDF export and print | Done | Commits `c04b2a4`…`59b2344`. Print goes through the desktop's print portal. See §8 |
| M6 Packaging and Omarchy docs | Done | Commit `332ad1f`: `docs/omarchy.md`, the icon, keywords, `pkgver` 0.2.0. See §8 |
| Release 0.2.0 | Done | Tag `omalorem-v0.2.0` at `332ad1f`, pushed. `bin/install` confirmed working by the owner |
| M7 Document links and history | Done | Commits `855a3e7`…`dd66240`, merged. See §8 and SPEC §4.5, §5.7 |
| Release 0.3.0 | Done | Tag `omalorem-v0.3.0` at `87f1a22`, pushed |
| M8 Preview → editor scroll sync | Done | Commit `b0dc4fe`, merged. See §8 and SPEC §4.4, §5.7 |
| M9 Omalorem's own text size | Done | Commit `abae9f5`, merged. See §8 and SPEC §4.3, §5.6.1 |
| Release 0.4.0 | Done | Tag `omalorem-v0.4.0`; `pkgver` 0.4.0 |
| M10 The wide column | Done, unreleased | Branch `m10-loose-measure`. See §8 and SPEC §4.6, §5.6.2 |

What works today:
- The preview opens as its own top-level window with no transient parent, so Hyprland tiles it beside the editor.
- `Ctrl+E` and the footer preview button show and hide it, and the setting is saved in `preview/visible`.
- Markdown (CommonMark and GFM tables, strikethrough, autolinks, task lists) and KaTeX math (`$`, `$$`, `\(…\)`, `\[…\]`) render from qrc, with no network access.
- Theme colours, light and dark mode, and the desktop text scale apply live.
- `Ctrl+Shift+T` switches the preview's prose between iA Writer Mono S and Quattro S, saved in `preview/font`. Code stays in Mono, and the column width doesn't change.
- The preview re-renders 120 ms after the last keystroke. Open, reload and recovery render at once.
- Rendering is block-keyed: unchanged blocks keep their DOM nodes and typeset math, and KaTeX output is cached. An update of the 2,000-line, 200-equation document takes a median 9.5 ms offscreen.
- `Ctrl+-` and `Ctrl+=` set Omalorem's own text size, 10% a step on top of the desktop's, and `Ctrl+0` returns to the desktop's. Both windows and the chrome follow; it is saved as `view/textScale`.
- `Ctrl+Shift+W` widens the column in both windows: a 24 px gutter instead of upstream's ten characters a side, and a measure of 80 characters instead of 65. Saved as `view/wideColumn`, off by default. The editor half works in `no_preview` builds; the preview gets the measure as `PreviewBridge::columnCharacters`.
- Scroll sync runs both ways, on a fractional source line: the window you last scrolled leads. The preview follows the editor until the reader scrolls the preview, and then the editor follows the preview until the editor is scrolled or typed in again.
- Local images in the document's folder and its subfolders load through `omalorem-doc:`. Remote, out-of-folder and unsaved-document images show a placeholder with the alt text.
- Headings have GitHub-style ids, so `#anchor` links scroll within the preview.
- Right-clicking the preview shows its own menu: Copy, Select all, Back, Forward.
- A link to a Markdown file in the document's folder opens it in the editor, with a dotted underline to mark it and the unsaved-changes dialog when needed; `notes.md#heading` scrolls there. `Alt+Left` / `Alt+Right` and the mouse's side buttons walk the last 20 documents, restoring the caret. The history works in `no_preview` builds too, where only `Ctrl+O` fills it.
- `Ctrl+Shift+P` exports a PDF, and `Ctrl+P` prints, both through the preview, so the math is rendered. The output uses the light palette and 20 mm margins. Export uses A4 or Letter by locale. Print opens the desktop's GTK print dialog through xdg-desktop-portal at once, renders on the paper chosen there, and sends a vector PDF; Qt's dialog is the fallback. Both work with the preview hidden: the preview window loads and stays hidden.
- Returning to the preview's workspace repaints it, instead of leaving it black.
- The editor highlights `$…$`, `$$…$$`, `\(…\)` and `\[…\]`, but not inside fenced code. Emphasis characters inside math (`x_1`) stay visible and the caret doesn't skip them. `Ctrl+M` and `Ctrl+Shift+M` insert inline and display math, each as one undo step. Return inside an open display block adds a plain newline. All of this works in `no_preview` builds too.
- Closing the preview only hides it. Closing the editor closes both.
- `Ctrl+F` and `Ctrl+H` from the preview bring the editor forward. F11 in the preview fullscreens the preview.
- With the preview hidden, the `omalorem` binary never loads QtWebEngine: `ldd` shows no WebEngine library, and none is mapped at runtime.
- `no_preview` builds are plain Omawrite with the Omalorem name.

Tests: `bin/test` passes both targets. `tst_omalorem` has 40 passed. `tst_preview` has 39 passed, with no expected failures. `patchesOnlyChangedBlocks` logs the render timing (§5).

## 2. Rules for every change

These come from the spec and from decisions made with the project owner. Don't break them without a spec change.

1. **Additive-only upstream diff (SPEC §2, principle 1).** Files that came from upstream Omawrite may only gain lines. `README.md` and `pkgbuild/` are the fork's own, like `docs/`: they carry Omalorem's identity, so they are rewritten as needed and left out of the audit. No existing upstream line is modified or deleted. The exceptions are the `Ctrl+?` reference text in `src/Main.qml` and the lines that carry the project's name (renamed at the fork, and again from Omaview to Omalorem). Preview behaviour lives in the preview's own files and reaches the editor through additive hooks: new properties, functions, blocks and signals. The reason is to keep upstream rebases clean and keep the preview portable to another editor. The README still keeps upstream's shortcut and requirement lines word for word, to keep rebases small. Check with the audit in §5.
2. **Nothing costs anything until it's used.** WebEngine code belongs only in the `Omalorem.Preview` plugin (`src/previewplugin/`), never in the executable. Don't call `QtWebEngineQuick::initialize()` from `main.cpp`: that links WebEngine and cost +26% cold start in M1.
3. **Offline and sandboxed.** All web assets are vendored in `third_party/` (fonts in `fonts/`) and compiled in through qrc. Follow the update procedure in `third_party/VERSIONS`, and never commit `node_modules` or `package.json`. The request interceptor allows only `qrc:` and `omalorem-doc:` inside the document's folder; `file:` is always refused.
4. **Match the code style.** Use `QStringLiteral`, `Q_PROPERTY` with NOTIFY, the `m_` prefix, 4-space indents and C++17. In QML, sizes go through `win.scaledSize()` and colours come from `backend.theme*`. Comments explain *why*.
5. **Every behaviour change gets a QtTest case.** Keep all tests green, and report real test output.
6. **Commit one phase at a time** on a milestone branch. Each commit ends with the `Co-Authored-By` trailer. Don't push without being asked.
7. **Don't drive the user's live desktop** from an agent: no `wtype` or other key injection, no moving windows between workspaces, no screenshot tools. In M1 a test script sent keystrokes to the wrong window. Test offscreen, and list what a human needs to check on Hyprland. Never change Hyprland or Omarchy config permanently.

## 3. Code layout

```
src/
  main.cpp, Main.qml, backend.*, …    upstream editor (additive changes only); Main.qml has
                                      "editor math" and "preview" blocks, backend.cpp an Omalorem section
  markdownhighlighter.*               upstream highlighter, plus the math scanner (mathSpans, MathState)
  EditorMath.js                       Ctrl+M and Ctrl+Shift+M edits, as pure functions
  previewbridge.{h,cpp}               PreviewBridge: plain QObject, no WebEngine dependency
  previewpolicy.h                     header-only URL policy and omalorem-doc: path mapping, shared by the bridge, plugin and tests
  PreviewHost.qml, previewhost.qrc    `import Omalorem.Preview; PreviewWindow {}`; this import loads the plugin
  preview/                            index.html, preview.css, preview.js (the page)
  previewplugin/                      Omalorem.Preview QML plugin; everything that needs QtWebEngine
    previewplugin.pro, qmldir, previewplugin.cpp, previewplugin.qrc
    previewsandbox.{h,cpp}            PreviewSandbox singleton: request interceptor, omalorem-doc: scheme
                                      handler, PreviewExposeWatcher, print portal client, QPrintDialog fallback
    PreviewPane.qml                   WebEngineView wrapper (profile prototype, navigation blocking, repaint)
    PreviewWindow.qml                 top-level window: title, close-to-hide, find focus, F11, repaint on
                                      return, PDF export and print (produce)
third_party/                          markdown-it 14.3.2, markdown-it-texmath 1.0.0, katex 0.16.47,
                                      qwebchannel.js (Qt 6.11.2); VERSIONS has the pins and sha256s
fonts/                                upstream's Mono S (.ttf, editor), plus Quattro S (.woff2, preview plugin only)
tests/
  tst_omalorem.cpp, tests.pro          editor, backend and bridge tests (no WebEngine)
  preview/                            tst_preview: real WebEngineView offscreen, plus the integration tests
  fixtures/                           sample, math-valid, math-invalid, currency, code, raw-html, remote .md, pixel.png
pkgbuild/                             PKGBUILD (installs the binary and the plugin), desktop file, icon, install hook
bin/
  build, test, install                upstream scripts; bin/test has appended lines for the preview tests
  build-no-preview                    builds the no_preview variant into build-no-preview/
```

### How the pieces connect

```
Main.qml TextEdit ──textChanged──▶ backend.previewEditorTextChanged()   (additive hook)
Main.qml editorFlick.contentY ──▶ backend.previewLineAt() ──▶ bridge.sourceLine   (scroll sync, while wanted)
Backend ──▶ PreviewBridge (120 ms debounce; open/reload/recovery render at once)
Backend's theme, darkMode, textScale and fileUrl signals ──▶ bridge properties
Main.qml Loader (after the editor's first frame, while previewVisible)
   └─▶ PreviewHost.qml ──import──▶ Omalorem.Preview plugin (loads WebEngine now)
          └─▶ PreviewWindow ─▶ PreviewPane ─▶ WebEngineView ─QWebChannel─▶ qrc:/preview/index.html
                                                                           markdown-it + texmath → KaTeX
                                                        omalorem-doc:/… ◀── images, via PreviewDocumentSchemeHandler

Ctrl+Shift+P ──▶ backend.previewExportPdfDialog() ──▶ PDF FileDialog ──▶ previewIntegration.requestOutput()
Ctrl+P ──▶ Backend::printDocument() ──(preview available)──▶ previewPrintRequested ──▶ requestOutput()
requestOutput() ──▶ loads PreviewWindow if needed (hidden stays hidden) ──▶ PreviewWindow.produce()
   print: PreviewSandbox.preparePortalPrint (GTK dialog) ──▶ render on the chosen paper ──▶ portalPrint(PDF)
   pdf:   flush text ──▶ wait for bridge.renderedRevision ──▶ preparePrint ──▶ printToPdf ──▶ status
```

- `main.cpp` sets `Qt::AA_ShareOpenGLContexts` before `QApplication`. It adds the plugin's import path, which is `build/` first, then `<bindir>/../lib/omalorem/qml`, and calls `setPreviewAvailable(true)` only if the plugin's `qmldir` exists. Without the plugin, the app runs as plain Omawrite.
- `PreviewPane` builds an off-the-record profile from a `WebEngineProfilePrototype` with no storage name, and `PreviewSandbox.protect()` attaches the interceptor. The `WebEngineView` is created by a `Loader` only after the pane completes, because the prototype returns a null profile before then, and `setProfile(nullptr)` segfaults.
- Find from the preview: `PreviewWindow` watches the editor's `searchOpen` and calls `editorWindow.requestActivate()`.
- F11 in the preview: a focus wrapper accepts F11's `ShortcutOverride`, which bubbles up even out of Chromium, and toggles fullscreen in that same handler. The key press itself never comes back out of the web view.
- **Rendering** (`preview.js`): `md.parse`, then the tokens are split into top-level blocks and each is rendered to HTML. That HTML is the block's key, and unchanged blocks keep their nodes. `data-source-line` and `data-source-line-end` are set on the nodes afterwards, outside the key. `window.omaloremPreview.lastRender` holds `{blocks, created, ms}` for the tests.
- **Scroll sync:** Main.qml probes the editor at `contentY` and publishes line plus fraction. The page maps it through `positionOfLine()` and puts it `paddingTop` below the view's top. A `following` flag decides whether renders re-apply the sync. The channel's `propertyUpdateInterval` is 16 ms, down from the default 50 ms. The other way (M8), the page publishes `lineAtTop()` through `scrolledTo()` while it isn't following, and Main.qml's `applyPreviewLine()` scrolls the editor with `applyingPreviewLine` set, so the move isn't published back.
- **Images:** the plugin registers `omalorem-doc:` in `registerTypes()`, which is before any profile exists and so is not too late. The scheme is not `LocalScheme`, because Chromium doesn't count `qrc:` as local and refuses the load with "Not allowed to load local resource". markdown-it's `validateLink` is widened to let `file:` through, so in-folder `file:` images work; the bridge still refuses `file:` links.
- **Output** (M5): the page acknowledges each markdown revision once fonts and images have loaded (`PreviewBridge::rendered`), and output waits for it. Margins come from cloned body padding in `@media print`, because Qt's `printToPdf` passes zero margins and Chromium then ignores `@page`.
- Debug logging: `QT_LOGGING_RULES="omalorem.preview.info=true"` logs the first render. `omalorem.preview.sandbox.debug=true` logs blocked requests and print-portal availability. `omalorem.preview.expose.debug=true` logs the preview window's expose events.

## 4. Build, run and test

```sh
bin/build                         # build/omalorem plus the plugin in build/Omalorem/Preview/
build/omalorem tests/fixtures/sample.md
bin/build-no-preview              # build-no-preview/omalorem, with no WebEngine and no plugin
bin/test                          # tst_omalorem, then tst_preview (offscreen)
```

- `tst_preview` needs no Chromium flags on a machine with working GL. Where GL is missing, set `QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu"`.
- Harmless noise in the test output: "Binding loop … Dialog" warnings, which come from upstream's `Main.qml`, and one "Fontconfig error" line during the interceptor test.

## 5. Checks worth repeating before each merge

```sh
# The executable must not link WebEngine
ldd build/omalorem | grep -iE 'webengine|webchannel'          # expect no output

# Additive-only audit against the fork commit. -M5% follows the renamed files
# (omaview.pro, tst_omaview.cpp, pkgbuild/omaview.*): they have grown several
# times over since the fork, so git's default 50% misses them, and the
# threshold has to drop as they grow further — if the audit suddenly reports a
# whole file as removed, that is what happened, not a deleted line. Lines
# carrying the old name belong to the rename and are filtered out.
git diff -M5% eb6bd1a HEAD -- . ':!docs' ':!third_party' ':!README.md' ':!pkgbuild' \
  | grep -E '^-[^-]' | grep -vi omaview                       # expect only the old Ctrl+? text line
# To confirm the filtered lines changed only the name: swap omaview for
# omalorem (all three cases) in each one; each must then appear as an added
# line. At the rename, all 36 did.
```

Timing method: offscreen, isolated settings, launch to the editor's first frame, median of 10–15 runs. Results at M1.1:

| Case | Result | Target |
|---|---|---|
| Preview hidden, against `no_preview` | about 281 ms each (≈0%) | ≤10% — met |
| Preview first render after the editor's frame | 732 ms (offscreen, no GPU) | ≤600 ms — open (SPEC §10 Q3) |

M2 did not re-time startup. Its Main.qml additions are `Connections` that are disabled while the preview is hidden, and a shortcut.

Render budget at M2, from `tst_preview`'s log, offscreen with no GPU:

| Case | Result | Target |
|---|---|---|
| Update, 2,000 lines and 200 equations | median 9.5 ms, worst 12 ms | ≤16 ms — met |
| First update after load (KaTeX fonts arrive, one full relayout) | about 115 ms | one-off, not budgeted |
| Cold render of the same document | about 310 ms | not budgeted |

## 6. Known issues and open questions

- **First-render time** (SPEC §10 Q3). This needs measuring on Hyprland with the GPU. If it still misses, relax the target. Don't warm WebEngine up on a background thread, because that isn't safe.
- **Image caching.** Chromium may cache an `omalorem-doc:` image for the session, so replacing an image file on disk may not show until the document is reopened. This hasn't been checked.
- **Scrollbar drags in the preview** may not count as "the reader scrolled". Wheel, keys, touch and pointer-down do, but whether Chromium sends `pointerdown` for a scrollbar drag hasn't been checked. If not, a render while the preview is scrolled that way would snap it back to the editor's line.
- **`Ctrl+F` from the preview while find is already open** leaves focus in the preview. This is accepted, to keep the editor's find handlers untouched.
- **Profile lifetime.** The profile lives only as long as its `PreviewPane`. That's fine with one pane for the app's life, and M5 prints from the same (possibly hidden) pane. Decide before anything creates a second pane, such as the future docked placement (SPEC §5.3, §11): the profile would probably move into `PreviewSandbox`.
- **Dormant placement setting.** Backend's `previewPlacement` and `preview/placement`, with their test, remain from M1. Only `window` is used. They are kept for the future docked placement, but could be removed if that feature is dropped for good.
- **Full rehighlights are slower with math (M4).** Typing re-highlights only the changed lines, so it isn't affected: 14 ms to load the 2,000-line, 300-formula document, and 23 ms when a new `$$` at the top flips every line below. A full `rehighlight()` is another matter; Omawrite calls it on theme and dark-mode changes and on every find update. Upstream takes about 295 ms on that document, and M4 takes about 440 ms. Scanning isn't the cost: without the math formats it's 240 ms, faster than upstream, because formula underscores are no longer hidden. The cost is laying out the extra format ranges, about 150 ms for 900 ranges, and opacity makes no difference. Colouring content only, without separate delimiter ranges, would save about 90 ms. The spec's muted delimiters were kept.
- **Black preview after a workspace switch: fixed, and confirmed by the owner on Hyprland.** The view stayed black until the page painted again, for example on a scroll. The fix asks the page for a fresh frame: a two-frame opacity of 0.9999, which can't be seen. It does this when the preview window is exposed again (`PreviewExposeWatcher`) and whenever either window becomes active, retrying once after 150 ms. If it ever comes back, run with `QT_LOGGING_RULES="omalorem.preview.expose.debug=true"` to see which window events arrive.
- **The portal print dialog has no parent window.** Qt has no portable way to name a Wayland parent to the portal, so the GTK dialog is its own window. Whether Hyprland tiles or floats it may want a window rule in `docs/omarchy.md` (M6).
- **Omawrite's own `Ctrl+P` is still slow in `no_preview` builds.** It uses Qt's `QPrintDialog`, which waited about 10 s on CUPS printer discovery on the owner's machine. The portal path only applies with the preview.
- **Confirmed by the owner on Hyprland:** the preview in everyday use; editor math; the black-preview fix; `Ctrl+P` through the portal, with its dialog opening at once; `bin/install` end to end, with the 0.2.0 package.
- **Not yet checked by a human on Hyprland:**
  - fractional scaling at 1.25 and 1.5
  - the checklist in SPEC §7, repeated after each UI milestone
  - M5: the portal save dialog for `Ctrl+Shift+P`; one real printout on paper; export with the preview hidden
  - M4: math colours in light and dark themes; `Ctrl+M` and `Ctrl+Shift+M`; Return in a `$$` block
  - M2: that scroll sync feels smooth with the editor's wheel animation and with a GPU; that Quattro looks right; that a local image beside a saved document shows; and the scrollbar-drag question above

## 7. Next: a release for M10

M10 is built and unreleased, so the next step is 0.5.0 by the recipe below. SPEC §11 holds the future features, §10 the open questions, and §6 what still wants checking by hand on Hyprland.

Also open, and worth settling before publishing anywhere: upstream Omawrite has moved to 0.7.0 while the fork sits at 0.5.0 (`8f98892`), and `pkgbuild/PKGBUILD` builds from `$startdir/..` with an empty `source=()`, which neither the AUR nor the Omarchy package repository accepts.

### Done: M10, the wide column

`Ctrl+Shift+W` gives the sides back to the text in both windows: a 24 px gutter and an 80-character measure. Scope in SPEC §4.6 and §5.6.2. Nothing of Omawrite's layout is edited: a `Binding` with `when:` overrides upstream's `editorWidth` and Qt restores it when the mode goes off.

### Done: M8, the preview leading the editor

Scroll sync in the other direction, so reading down the preview brings the editor along and the passage just read is ready to edit. Scope in SPEC §4.4 and §5.7. Built on the `following` flag the preview already keeps: when the reader has taken the preview off the editor's lead, the page publishes its own top line instead.

After it: nothing is planned, and 0.4.0 would be the release.

**How a release is made**, for the next one:
1. Check the tree: `bin/test` green, `bin/build-no-preview` builds, `ldd build/omalorem` shows no WebEngine, and the additive audit in §5 is clean.
2. Bump `pkgver` in `pkgbuild/PKGBUILD`, and update the version in the README's status line, SPEC's header and these notes.
3. Commit that as the release commit, and push `main`.
4. `bin/install`, and use it for a moment.
5. `git tag -a omalorem-v<version>` with the release note as the message, then `git push origin omalorem-v<version>`. The tags carry the name, because upstream's `v…` tags live in this repository too.

### After the release

0.2.0 is released and M0–M6 are done (M3 aside, which moved to SPEC §11). What is open after M7, in the order it is worth doing:
- **Small checks on Hyprland** (§6): the first-render time with a GPU against the 600 ms target in SPEC §10 Q3; fractional scaling at 1.25 and 1.5; whether a replaced image file shows without reopening the document; and whether dragging the preview's scrollbar stops it following the editor.
- **Future features, only if users ask** (SPEC §11): the docked placement, proper handling of wide formulas in print, and page options such as page numbers and link targets.
- **Keeping up with upstream:** rebase onto Omawrite when it moves. The audit in §5 is what keeps that cheap.

For a next release, bump `pkgver` in `pkgbuild/PKGBUILD`, then tag `omalorem-v<version>` and push the tag.

## 8. History

### Planning (2026-09-21)
- M1 was planned in six phases: setup/vendoring, bridge, lazy WebEngine and sandbox, page, QML, tests.
- Spec v0.3 (`52b6bde`):
  - M1 includes the footer button and the `no_preview` scope, and does whole-`#content` rendering.
  - `markdown-it-texmath` was chosen over `@vscode/markdown-it-katex` because it supports `\(…\)` and `\[…\]`.

### M1: Pop-out preview window
| Phase | Commit | What |
|---|---|---|
| 0 | `b908f24` | Vendored markdown-it, texmath, KaTeX and qwebchannel with pinned versions and licences |
| 1 | `91e5bc0` | `PreviewBridge`, and Backend's `previewVisible`, `previewPlacement` and `previewAvailable`, with 7 tests |
| 2 | `028450a` | WebEngine init, off-the-record profile and interceptor, and the `no_preview` scope |
| 3 | `8c770b5` | Page: markdown-it, texmath and KaTeX, theme variables, text scale, `data-source-line`, task lists, CSP |
| 4 | `1327d12` | `PreviewPane`, `PreviewWindow`, the `Ctrl+E` Loader and the footer preview icon |
| 5 | `96906ed` | Fixtures and the `tst_preview` render and integration tests, plus startup timing |

Findings:
- Qt gives a second `Window` the first window as its transient parent automatically. `PreviewWindow` sets `transientParent: null` explicitly so that Hyprland tiles it.
- Repeating the application-wide `Shortcut`s in `PreviewWindow` made Qt treat them as ambiguous, and neither fired. The shortcuts in `Main.qml` already work from the preview window.
- texmath let an inline `$…$` span run across other `$` signs, so `cost $5 and $10, or $x$` broke. `preview.js` tightens the rule; the vendored file is unchanged.
- Cold start with the preview hidden was +26% against `no_preview`, because linking `libQt6WebEngineCore` alone costs about 90 ms.

### Spec v0.4 (`5c3ad4c`): the owner's decisions after M1
- The startup fix became M1.1: the preview moved into a QML plugin, and the early `initialize()` call was dropped.
- The additive-only rule for upstream files was added to principle 1.
- The spec now says `PreviewWindow` doesn't repeat shortcuts.
- Local images go through a custom scheme in M2.

### M1.1: Lazy WebEngine and additive-only diff
| Commit | What |
|---|---|
| `fe28c62` | Reverted the M1 edits to `toggleFullScreen`, the find handlers and `openExternalUrl`, and moved find and F11 into the preview. Made `.pro` and script changes append-only, and added `bin/build-no-preview`. |
| `22db1c8` | Created the `Omalorem.Preview` plugin. The binary no longer links WebEngine. Added `WebEngineProfilePrototype`, which removed the Qt 6.11 deprecation warning, and a staged-install check. |

Result:
- Cold start with the preview hidden is now on par with `no_preview`.
- The upstream diff is 540 lines added and 1 line changed (the `Ctrl+?` text).
- The first render slowed to 732 ms offscreen, because WebEngine now loads on first show.

### Spec v0.5 (`a70fc5d`)
- Recorded the plugin file layout and the M6 packaging steps.
- Recorded the profile-lifetime note for M3 and the accepted `Ctrl+F` limit.
- Marked M1 and M1.1 done, and added the first-render target as an open question.

### M2: Fonts, incremental render, scroll sync and images
| Commit | What |
|---|---|
| `686f38d` | Quattro S woff2 in the plugin, `previewFont` and `preview/font`, and `Ctrl+Shift+T`. `#content` stays in Mono so `65ch` doesn't change |
| `78c73f8` | Block-keyed DOM patching, the KaTeX cache, `window.omaloremPreview` stats, and the 2,000-line performance test |
| `c35d24e` | `omalorem-doc:` scheme and handler, the resource policy without `file:`, image placeholders, and turning the expected failure into a pass |
| `be36447` | Heading ids for `#anchor` links |
| `2acb47c` | Editor→preview scroll sync: fractional `sourceLine`, `Backend::previewLineAt`, and the page's follow mode |

Findings:
- Chromium doesn't treat `qrc:` as a local scheme. Registering `omalorem-doc:` with `LocalScheme` made the page refuse it ("Not allowed to load local resource").
- markdown-it's default `validateLink` drops `file:` URLs, so a `file:` image never becomes an `<img>` unless that check is widened.
- Checking whether the preview is following by comparing `scrollTop` against the last synced value is fragile, because scroll anchoring and late fonts move it. An explicit flag, driven by input events, works reliably.
- Opening a file leaves upstream's editor scrolled to the end, so `sourceLine` starts at the last line, not 0.
- With whole-document rendering, the first update after load was dominated by layout, because KaTeX's fonts arrive after the first paint. Patching doesn't change that one-off.

### Release 0.1.0 and the rename to Omalorem
- `omaview-v0.1.0` is an annotated tag on `344343b`, the M2 docs commit. It isn't a bare `v0.1.0`, because upstream's tags `v0.1.0`…`v0.5.0` are in this repository too.
- The next commit renames every spelling (`omaview`, `Omaview`, `OMAVIEW`), with these results:
  - Files: `omalorem.pro`, `tests/tst_omalorem.cpp`, and `pkgbuild/omalorem.{desktop,install,svg}`.
  - Runtime names: the binary, desktop entry, icon, `QSettings` application name and recovery directory, the QML module `Omalorem.Preview` and its install path `/usr/lib/omalorem/qml`, the `omalorem.preview*` logging categories, and the `omalorem-doc:` scheme.
- There is no settings migration. Nothing was installed under the old name beyond development builds, so `~/.config/omaview` is simply left behind.
- The name comes from *lorem ipsum*, placeholder text, because the project is a small extension to Omawrite.

### Spec v0.8: Omarchy and tiling window managers only
- The docked placement (M3) moved to SPEC §11, future features, with its design intact. `Ctrl+Shift+E` is unassigned, and `preview/placement` stays dormant.

### M4: Editor math support
All of this is additive: new functions, new lines inside `highlightBlock`, `highlightInline`, `hiddenRangesAt` and `smartReturn`, and a `Main.qml` block.

| Part | What |
|---|---|
| `MarkdownHighlighter::mathSpans`, `MathState`, `touchesMath`, `highlightMath` | The line scanner, block state for display math and fences, the math format, and dropping emphasis whose markers fall inside math |
| `Backend::hiddenRangesAt` (added lines) | The caret no longer skips `_`/`*` inside formulas |
| `Backend::displayMathOpenAt`, `smartReturn` (added lines) | Return in an open `$$` or `\[` block adds a plain newline |
| `src/EditorMath.js`, `Backend::replaceTextAsOneEdit`, Main.qml "editor math" block | `Ctrl+M` and `Ctrl+Shift+M`, one undo step each, in every build |
| `tst_omalorem` | `findsMathSpans`, `highlightsMath`, `keepsCaretOnMathMarkers`, `buildsMathEdits`, `editsMathFromTheKeyboard` |

Findings:
- Omawrite's emphasis rules were hiding and skipping underscores and asterisks inside formulas: `$x_1 + y_2$` showed as `x1 + y2`. The preview was right, so this only showed in the editor.
- A bare `QTextDocument` keeps no highlighter formats until `documentLayout()` has been created. The editor's document always has one; the unit test creates it.
- `EditorMutations.replaceRange` removes and then inserts, which is two undo steps. The math edits use a `QTextCursor` edit block instead. Omawrite's `Ctrl+B` keeps its two steps.
- `QList::append` with a nested braced initializer is ambiguous for aggregate structs, so the scanner builds spans through a typed helper.

### Fix: black preview after a workspace switch
- Reported on Hyprland: returning to the workspace left the preview black until a scroll. The likely cause is that the compositor stops the hidden window, Qt drops the last frame Chromium gave it, and nothing asks Chromium for another.
- `preview.js` gains `omaloremPreview.repaint()`, and `PreviewPane` gains `repaint()` and `repaintRequests`. `PreviewWindow` calls `repaint()` on re-exposure, through the new `PreviewExposeWatcher` type, and when either window becomes active, retrying once after 150 ms.
- Tests: `repaintsOnRequest`, `watchesForReexposure` and `repaintsPreviewWhenItComesBack`. All the changes are in preview files; no upstream file is touched.

### M5: PDF export and print
| Part | What |
|---|---|
| `PreviewBridge` | `markdownRevision`, `renderedRevision`, `rendered()`, and `pageReady` as a real Q_PROPERTY |
| `Backend` | Lines added at the top of `printDocument()` hand print to the preview. `previewExportPdfDialog()`, `previewFlushMarkdown()` and `previewReportStatus()` are new |
| Main.qml preview block | `Ctrl+Shift+P`, the PDF `FileDialog`, and `requestOutput()`, which loads the preview window (hidden if need be) and hands the request over |
| `PreviewWindow.produce()` | Flush, wait for the current revision, `preparePrint`, `printToPdf`, then report or print, with a 30 s timeout |
| `PreviewSandbox` | Portal print (`preparePortalPrint`, `portalPrint`, `paperFromPageSetup`, and `PreviewPortalRequest` for each Response), the `printPdf()` fallback (`QPrintDialog`, pages drawn at up to 300 dpi), temporary PDFs, and the `printTestTarget` and `portalService` test hooks. The plugin adds `pdf printsupport widgets dbus` |
| `preview.js` / `preview.css` | Render acknowledgement after fonts and images, `preparePrint()` for wide formulas, and the print stylesheet |
| Tests | `tracksRenderedRevision`, `routesPrintThroughPreview`, `suggestsPdfBesideDocument`, `scalesWideMathForPrint`, `exportsPdfWithRenderedMath` (hidden preview, several pages, margins on each, white paper, typed text included), `printsThroughThePreview` (fallback), `printsThroughThePrintPortal` (a mock portal on its own thread and connection: landscape A4, Letter, the token, vector text, cancel) |

Findings:
- `PreviewBridge::pageReady` had a getter and a signal but no Q_PROPERTY, so QML read `undefined` and output waited forever. It is a property now.
- Qt's QML `printToPdf` passes zero margins, and Chromium then ignores an `@page` margin. Cloned body padding gives every page its margins; the export test checks all four sides of every page.
- A preview window that is loaded but never shown still renders and prints: the page, WebChannel and `printToPdf` all work without the window being visible.
- `Ctrl+P` was slow to show its dialog: about 10 s on this machine, with no printers configured. Our own render took about 0.5 s. The rest was Qt's CUPS backend: about 1 s to list printers, 2 s to create a `QPrinter`, and 7 s for `QPrintDialog`, all in exact 1.02 s steps. That points to libcups waiting a full second for Avahi-announced network printers on each query. Pointing `CUPS_SERVER` at the local server changed nothing. The fix is the desktop's print portal, whose GTK dialog opens at once and fills in printers as it finds them.
- A mock portal on the same thread as the client would deadlock the client's blocking availability check, so the test runs it on a `QThread` with its own bus connection.
- The preview tests' new editor-based cases close their editor in a `qScopeGuard`. A failing check used to leave a hidden preview behind and fail the tests after it.

### Merged and pushed
- M4 (`71ce263`, `e0ab290`), the black-preview fix (`c625da3`), and M5 with portal print (`c04b2a4`, `273dc75`, `59b2344`) are on `main` and pushed. The owner confirmed the black-preview fix and portal printing on Hyprland.

### M6: Packaging and Omarchy docs
| Part | What |
|---|---|
| `docs/omarchy.md` | What works without configuration, optional Lua snippets for Hyprland 0.56 on Omarchy 4, launcher entry and keybinding, themes and text size, and troubleshooting |
| `pkgbuild/omalorem.svg` | A new icon: the same page as Omawrite's, with a square root in the accent blue. Checked at 256, 64 and 32 px |
| `pkgbuild/omalorem.desktop` | Keywords gained latex, math, katex and preview |
| `pkgbuild/PKGBUILD` | `pkgver` 0.2.0, the first release under the new name |

Findings:
- Omarchy 4 configures Hyprland in **Lua** (`~/.config/hypr/*.lua`), not `.conf`, and wraps window rules in `o.window(match, rules)`. The rule names come from Hyprland 0.56's Lua API (`/usr/share/hypr/stubs/hl.meta.lua`) and Omarchy's own examples in `/usr/share/omarchy/default/hypr/`; `no_initial_focus` is the one for "don't take focus when it opens".
- Two things Omalorem relies on are already Omarchy defaults: portal windows float and centre (`xdg-desktop-portal-gtk`), and `misc.focus_on_activate = true` lets the editor keep the keyboard when the preview appears.
- `desktop-file-validate` hints that `TextEditor` could be paired with `Utility`, but adding `Utility` makes it warn about two main categories, and the app could then appear twice in menus. Upstream's categories were kept.
- The packaging files carry the app's identity and can't stay additive-only against Omawrite's, so `pkgbuild/` joined `README.md` as the fork's own (SPEC §9).

### Release 0.2.0
- `omalorem-v0.2.0` is an annotated tag on `332ad1f`, pushed. Its message is the release note: the rename, the packaged preview plugin, editor math, PDF export and printing through the print portal, the black-preview fix, the narrowed scope, and the Omarchy notes and icon.
- The owner confirmed `bin/install` end to end with the 0.2.0 package.

### M7: Document links and history
| Part | What |
|---|---|
| `src/previewpolicy.h` | `previewFolderFilePath` (the containment rule, shared with the images), `isPreviewMarkdownPath`, and `previewLinkedDocumentPath`: exists, is a file, `.md`/`.markdown`, symlinks resolved |
| `PreviewBridge` | `openDocument(href)` resolves against `baseUrl` and emits `documentLinkRequested(url, fragment)` or `documentLinkRefused(reason)`; `navigateBack/Forward`; `scrollTo(fragment)` for the page |
| `Backend` | The visited documents: `previewHistoryPrepare/Visited/Go`, `previewCanGoBack/Forward`, `previewHistoryRestored(caret)`. 20 deep, forward dropped on a new visit, nothing recorded for a document that never opens |
| `preview.js` / `preview.css` | Marks followable links with a `document-link` class as they render, draws them with a dotted underline, routes their clicks to `openDocument`, and sends the mouse's side buttons to the history |
| `PreviewPane` / `PreviewWindow` | The preview's own context menu: Copy, Select all, Back, Forward. Chromium's is suppressed by accepting `contextMenuRequested` |
| `Main.qml` | A "document history" block (in every build): the two shortcuts, recording visits on `fileUrlChanged`, the caret before `Ctrl+O`, and restoring it. The preview block connects the bridge's link signals to `win.requestOpen()` and applies the fragment once the new document has rendered |
| Tests | `followsOnlyMarkdownLinksInFolder`, `resolvesDocumentLinks`, `keepsDocumentHistory`, `walksDocumentHistoryFromTheKeyboard` (no preview), `marksAndFollowsDocumentLinks` (the page), `opensLinkedDocumentsInTheEditor` (the whole path, including the unsaved-changes dialog and the fragment) |

Findings:
- Nothing upstream needed changing: `win.requestOpen()` already handles unsaved changes, and `Backend::open()` already re-points the preview, rewatches the file and clears recovery.
- The page can't know whether a file exists, so it marks anything in-folder that ends in `.md`. C++ decides, and a missing neighbour gets its own message. A link with a dotted underline is therefore a promise about the rule, not about the file.
- Recording a visit on `fileUrlChanged` covers every way a document opens, including `Ctrl+O` and the footer button. The caret to remember comes from the block, which hears `openDialogRequested` in time.
- The fragment needs the new document to be on the page first; M5's render revision already says when that is.
- Chromium's context menu offers Back, Forward, Reload and Save page, all permanently greyed out here, since the view loads one page and blocks navigation. The owner spotted them; the preview now shows its own menu, and its Back and Forward drive the document history. A real right-click in the test proves Chromium's menu is gone.

### M8: Preview → editor scroll sync
| Part | What |
|---|---|
| `preview.js` | `lineAtTop()`, the inverse of `positionOfLine()`, and a scroll listener that publishes it at most once a frame while the page isn't following the editor |
| `PreviewBridge` | `scrolledTo(line)` → `previewScrolled(line)`. It deliberately does not touch `sourceLine`, which stays the editor's own |
| `Backend` | `previewLineBounds(line)`: where a source line starts and ends, the inverse of `previewLineAt` |
| `Main.qml` | `applyPreviewLine()` scrolls the editor to that line, guarded by `applyingPreviewLine` so `updateSourceLine()` doesn't publish the move back |
| Tests | `mapsSourceLinesToDocumentPositions`, `reportsItsTopLineWhileTheReaderScrolls` (silent while following, reports when the reader leads, silent again once the editor scrolls), `scrollsTheEditorWithThePreview` (the editor's own probe agrees about the top line, and nothing is published back) |

Findings:
- The two directions are kept apart by one rule: the page publishes only while it isn't following, and the editor suppresses publishing only while applying. Everything else is the existing M2 machinery, inverted.
- `Qt.callLater(function() { ... })` did not clear the guard: with a throwaway closure the reset never ran, and the editor stayed silent for good. The property change is delivered while `scrollTo()` runs, so the guard is cleared on the next line instead. Worth remembering for any other "suppress while applying" flag in QML.

### M10: The wide column
| Part | What |
|---|---|
| `src/editorlayout.h` | `editorColumnWidth(characterWidth, windowWidth, wide)` and `editorColumnCharactersFor(wide)`, header-only and free of GUI types. In the unconditional `HEADERS`, not the preview scope, so `no_preview` builds get it |
| `Backend` | `wideColumn` (saved as `view/wideColumn`, off by default), `toggleWideColumn()`, and `editorColumnWidth()` as a `Q_INVOKABLE` for the QML binding |
| `Main.qml` | A wide-column block: a `Binding` on `editor.width` guarded by `when: backend.wideColumn`, and `Ctrl+Shift+W`, reporting "Wide column" / "Normal column" |
| `PreviewBridge` | `columnCharacters` (65 or 80), set in the lazy getter and on `wideColumnChanged`, so the preview follows without anything being built eagerly |
| The page | `preview.js` writes `--measure`; `#content` is `max-width: var(--measure, 65ch)`. Its 24 px padding already matched the editor's new gutter |
| Tests | `computesTheEditorColumn` (every band, both modes, the owner's own case), `remembersTheWideColumn` (default, saving, and that off leaves nothing behind), `widensTheColumnFromTheKeyboard` (the editor really resizes, and still follows the window afterwards) and `followsTheEditorsMeasure` in `tst_preview` |

Findings:
- The thick margin is not the 65-character measure. Upstream caps the column at the measure *and* reserves a flat 20 characters, 10 a side; in one wide window the reserve never binds, but between about 600 and 1020 px of window width it is the only term that binds and costs a fixed 120 px a side. Half of a 1536 px screen shows 43 characters.
- The preview never had the problem: `#content` is `max-width: 65ch` with a 24 px padding, so it keeps 24 px and gives the rest to the text. Side by side the editor carried a 120 px margin and the preview a 24 px one. Matching the preview is what the wide column does, so the change needed nothing on the preview side at all — no CSS, no bridge call.
- **A `Binding` with `when:` is what keeps this additive.** Upstream's `editorWidth` and the `TextEdit`'s `width: win.editorWidth` are both upstream lines and cannot be edited. Qt restores the original binding when `when` goes false, so switching the mode off gives back Omawrite's own layout rather than a copy of it — `widensTheColumnFromTheKeyboard` resizes the window afterwards to prove the binding is live and not a frozen number.
- **The first draft of the test was wrong about where the modes converge** (in the gutter-only version, since superseded). They were identical only above about 1020 px, not above 828: the shrunken gutter reached the full 65-character measure at 828 px, but upstream's own gutter held the column short until 1020. Writing the test is what caught it; the spec had said 828.
- Window resizes reach QML asynchronously under the offscreen platform, so the post-resize assertions need `QTRY_COMPARE`.
- No footer icon: principle 5 leaves the footer one extra icon and that is the preview toggle, as M7 already found for back and forward. The status line carries the feedback instead, as the text size does.
- **The first build shrank the gutter only, and did nothing for the owner.** At text size 0.7 in a 749 px window, 65 characters already fitted: the measure was binding, not the gutter, so `Ctrl+Shift+W` changed no pixel. The lesson is that there are two ways to waste a narrow window and the gutter is only one of them — once the measure binds, making the text smaller turns the space it frees into more margin, not more text. The wide column now moves both terms, 24 px and 80 characters, so it always does something above the floor.
- The preview needed exactly one new bridge property. `#content` already had the 24 px padding, so only the measure had to travel — `--measure` as a CSS custom property, in the same shape as `--base-size`.

### M9: Omalorem's own text size
| Part | What |
|---|---|
| `Backend` | `viewScale` (saved as `view/textScale`, rounded to tenths, clamped 0.6–1.5), `stepViewScale`, `resetViewScale`, and `applyViewScale(desktopScale)` — the policy, so it can be tested without running `main()` |
| `main.cpp` | Added lines that apply the product to the two things the desktop's scale already feeds: `applyInterfaceFont()` and `Backend::setTextScale()`. Connected after upstream's handler, so they have the last word |
| `Main.qml` | A text-size block with `Ctrl+-`, `Ctrl+=` (and `Ctrl++`) and `Ctrl+0`, reporting the percentage in the status line |
| Tests | `stepsItsOwnTextScale` (steps, clamps, what is saved, nonsense ignored, odd values rounded) and `changesTextSizeFromTheKeyboard` (the editor's font really changes, 20 → 18 → 22 → 20) |

Findings:
- The whole text-size chain already funnels through one value, so a factor applied at the source reaches the editor, the chrome and the preview with nothing else to change. The preview needed no code at all: it takes `textScale` from the bridge.
- Keeping it a factor rather than an absolute size means the desktop's setting still works, and `Ctrl+0` has something to return to.
- `applyViewScale()` exists so the arithmetic is testable: the wiring in `main.cpp` is two calls, and the tests drive the policy directly.
- Why this needed no Omawrite change: `main.cpp`'s upstream block leaves `applyInterfaceFont` and `backend` in scope, so the fork's added lines can reuse the very same calls.
