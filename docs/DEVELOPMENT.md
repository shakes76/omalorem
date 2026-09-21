# Omalorem — Development status

Last updated: 2026-09-21 · Branch: `m4-editor-math`, off `main` (not merged or pushed). Release: `omaview-v0.1.0`, which is the last commit under the old name

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
| M2 Fonts, incremental render, scroll sync, images | Done | Commits `686f38d`…`2acb47c` |
| M3 Docked placement | Deferred | Moved to SPEC §11, future features. Omalorem targets Omarchy and tiling window managers only |
| M4 Editor math support | Done | Branch `m4-editor-math`. See §8 |
| M5 PDF export and print | **Next** | See §7 |
| M6 Packaging and Omarchy docs | Started | The PKGBUILD installs the preview plugin and depends on qt6-webengine and qt6-webchannel (SPEC §6). `docs/omarchy.md` and an icon variant are still to do |

What works today:
- The preview opens as its own top-level window with no transient parent, so Hyprland tiles it beside the editor.
- `Ctrl+E` and the footer preview button show and hide it, and the setting is saved in `preview/visible`.
- Markdown (CommonMark and GFM tables, strikethrough, autolinks, task lists) and KaTeX math (`$`, `$$`, `\(…\)`, `\[…\]`) render from qrc, with no network access.
- Theme colours, light and dark mode, and the desktop text scale apply live.
- `Ctrl+Shift+T` switches the preview's prose between iA Writer Mono S and Quattro S, saved in `preview/font`. Code stays in Mono, and the column width doesn't change.
- The preview re-renders 120 ms after the last keystroke. Open, reload and recovery render at once.
- Rendering is block-keyed: unchanged blocks keep their DOM nodes and typeset math, and KaTeX output is cached. An update of the 2,000-line, 200-equation document takes a median 9.5 ms offscreen.
- Scroll sync from the editor to the preview uses a fractional `sourceLine`. The preview follows the editor until the reader scrolls the preview.
- Local images in the document's folder and its subfolders load through `omalorem-doc:`. Remote, out-of-folder and unsaved-document images show a placeholder with the alt text.
- Headings have GitHub-style ids, so `#anchor` links scroll within the preview.
- The editor highlights `$…$`, `$$…$$`, `\(…\)` and `\[…\]`, but not inside fenced code. Emphasis characters inside math (`x_1`) stay visible and the caret doesn't skip them. `Ctrl+M` and `Ctrl+Shift+M` insert inline and display math, each as one undo step. Return inside an open display block adds a plain newline. All of this works in `no_preview` builds too.
- Closing the preview only hides it. Closing the editor closes both.
- `Ctrl+F` and `Ctrl+H` from the preview bring the editor forward. F11 in the preview fullscreens the preview.
- With the preview hidden, the `omalorem` binary never loads QtWebEngine: `ldd` shows no WebEngine library, and none is mapped at runtime.
- `no_preview` builds are plain Omawrite with the Omalorem name.

Tests: `bin/test` passes both targets. `tst_omalorem` has 27 passed. `tst_preview` has 26 passed, with no expected failures. `patchesOnlyChangedBlocks` logs the render timing (§5).

## 2. Rules for every change

These come from the spec and from decisions made with the project owner. Don't break them without a spec change.

1. **Additive-only upstream diff (SPEC §2, principle 1).** Files that came from upstream Omawrite may only gain lines. No existing upstream line is modified or deleted. The exceptions are the `Ctrl+?` reference text in `src/Main.qml` and the lines that carry the project's name (renamed at the fork, and again from Omaview to Omalorem). Preview behaviour lives in the preview's own files and reaches the editor through additive hooks: new properties, functions, blocks and signals. The reason is to keep upstream rebases clean and keep the preview portable to another editor. `README.md` counts as the fork's own file, like `docs/`: it describes Omalorem, so it is rewritten as needed. It still keeps upstream's shortcut and requirement lines word for word, to keep rebases small. Check with the audit in §5.
2. **Nothing costs anything until it's used.** WebEngine code belongs only in the `Omalorem.Preview` plugin (`src/previewplugin/`), never in the executable. Don't call `QtWebEngineQuick::initialize()` from `main.cpp`: that links WebEngine and cost +26% cold start in M1.
3. **Offline and sandboxed.** All web assets are vendored in `third_party/` (fonts in `fonts/`) and compiled in through qrc. Follow the update procedure in `third_party/VERSIONS`, and never commit `node_modules` or `package.json`. The request interceptor allows only `qrc:` and `omalorem-doc:` inside the document's folder; `file:` is always refused.
4. **Match the code style.** Use `QStringLiteral`, `Q_PROPERTY` with NOTIFY, the `m_` prefix, 4-space indents and C++17. In QML, sizes go through `win.scaledSize()` and colours come from `backend.theme*`. Comments explain *why*.
5. **Every behaviour change gets a QtTest case.** Keep all tests green, and report real test output.
6. **Commit one phase at a time** on a milestone branch. Each commit ends with the `Co-Authored-By` trailer. Don't push without being asked.
7. **Don't drive the user's live desktop** from an agent: no `wtype` or other key injection, no moving windows between workspaces, no screenshot tools. In M1 a test script sent keystrokes to the wrong window. Test offscreen, and list what a human needs to check on Hyprland. Never change Hyprland or Omarchy config permanently.

## 3. Code layout

```
src/
  main.cpp, Main.qml, backend.*, …    upstream editor (additive changes only)
  previewbridge.{h,cpp}               PreviewBridge: plain QObject, no WebEngine dependency
  previewpolicy.h                     header-only URL policy and omalorem-doc: path mapping, shared by the bridge, plugin and tests
  PreviewHost.qml, previewhost.qrc    `import Omalorem.Preview; PreviewWindow {}`; this import loads the plugin
  preview/                            index.html, preview.css, preview.js (the page)
  previewplugin/                      Omalorem.Preview QML plugin; everything that needs QtWebEngine
    previewplugin.pro, qmldir, previewplugin.cpp, previewplugin.qrc
    previewsandbox.{h,cpp}            PreviewSandbox singleton, request interceptor, omalorem-doc: scheme handler
    PreviewPane.qml                   WebEngineView wrapper (profile prototype, navigation blocking)
    PreviewWindow.qml                 top-level window: title, close-to-hide, find focus, F11
third_party/                          markdown-it 14.3.2, markdown-it-texmath 1.0.0, katex 0.16.47,
                                      qwebchannel.js (Qt 6.11.2); VERSIONS has the pins and sha256s
fonts/                                upstream's Mono S (.ttf, editor), plus Quattro S (.woff2, preview plugin only)
tests/
  tst_omalorem.cpp, tests.pro          editor, backend and bridge tests (no WebEngine)
  preview/                            tst_preview: real WebEngineView offscreen, plus the integration tests
  fixtures/                           sample, math-valid, math-invalid, currency, code, raw-html, remote .md, pixel.png
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
```

- `main.cpp` sets `Qt::AA_ShareOpenGLContexts` before `QApplication`. It adds the plugin's import path, which is `build/` first, then `<bindir>/../lib/omalorem/qml`, and calls `setPreviewAvailable(true)` only if the plugin's `qmldir` exists. Without the plugin, the app runs as plain Omawrite.
- `PreviewPane` builds an off-the-record profile from a `WebEngineProfilePrototype` with no storage name, and `PreviewSandbox.protect()` attaches the interceptor. The `WebEngineView` is created by a `Loader` only after the pane completes, because the prototype returns a null profile before then, and `setProfile(nullptr)` segfaults.
- Find from the preview: `PreviewWindow` watches the editor's `searchOpen` and calls `editorWindow.requestActivate()`.
- F11 in the preview: a focus wrapper accepts F11's `ShortcutOverride`, which bubbles up even out of Chromium, and toggles fullscreen in that same handler. The key press itself never comes back out of the web view.
- **Rendering** (`preview.js`): `md.parse`, then the tokens are split into top-level blocks and each is rendered to HTML. That HTML is the block's key, and unchanged blocks keep their nodes. `data-source-line` and `data-source-line-end` are set on the nodes afterwards, outside the key. `window.omaloremPreview.lastRender` holds `{blocks, created, ms}` for the tests.
- **Scroll sync:** Main.qml probes the editor at `contentY` and publishes line plus fraction. The page maps it through `positionOfLine()` and puts it `paddingTop` below the view's top. A `following` flag decides whether renders re-apply the sync. The channel's `propertyUpdateInterval` is 16 ms, down from the default 50 ms.
- **Images:** the plugin registers `omalorem-doc:` in `registerTypes()`, which is before any profile exists and so is not too late. The scheme is not `LocalScheme`, because Chromium doesn't count `qrc:` as local and refuses the load with "Not allowed to load local resource". markdown-it's `validateLink` is widened to let `file:` through, so in-folder `file:` images work; the bridge still refuses `file:` links.
- Debug logging: `QT_LOGGING_RULES="omalorem.preview.info=true"` logs the first render. `omalorem.preview.sandbox` logs blocked requests.

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

# Additive-only audit against the fork commit. -M20% follows the renamed
# files (omaview.pro, tst_omaview.cpp, pkgbuild/omaview.*): the .pro and the
# test file have more than doubled since the fork, so git's default 50% misses
# them. Lines carrying the old name belong to the rename and are filtered out.
git diff -M20% eb6bd1a HEAD -- . ':!docs' ':!third_party' ':!README.md' \
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
- **Profile lifetime.** The profile lives only as long as its `PreviewPane`. That's fine with one pane for the app's life. Decide before anything creates a second pane, such as offscreen printing in M5 or the future docked placement (SPEC §5.3, §11): the profile would probably move into `PreviewSandbox`.
- **Dormant placement setting.** Backend's `previewPlacement` and `preview/placement`, with their test, remain from M1. Only `window` is used. They are kept for the future docked placement, but could be removed if that feature is dropped for good.
- **Full rehighlights are slower with math (M4).** Typing re-highlights only the changed lines, so it isn't affected: 14 ms to load the 2,000-line, 300-formula document, and 23 ms when a new `$$` at the top flips every line below. A full `rehighlight()` is another matter; Omawrite calls it on theme and dark-mode changes and on every find update. Upstream takes about 295 ms on that document, and M4 takes about 440 ms. Scanning isn't the cost: without the math formats it's 240 ms, faster than upstream, because formula underscores are no longer hidden. The cost is laying out the extra format ranges, about 150 ms for 900 ranges, and opacity makes no difference. Colouring content only, without separate delimiter ranges, would save about 90 ms. The spec's muted delimiters were kept.
- **Not yet checked by a human on Hyprland:**
  - fractional scaling at 1.25 and 1.5
  - the checklist in SPEC §7, repeated after each UI milestone
  - M4: math colours in light and dark themes; `Ctrl+M` and `Ctrl+Shift+M`; Return in a `$$` block
  - M2: that scroll sync feels smooth with the editor's wheel animation and with a GPU; that Quattro looks right; that a local image beside a saved document shows; and the scrollbar-drag question above

## 7. Next: M5, PDF export and print

This is scope from SPEC §4.2, §5.5 and §8:
- **`Ctrl+Shift+P` exports to PDF** through `WebEngineView.printToPdf`, with a portal save dialog that suggests `<name>.pdf`. A `@media print` stylesheet forces the light palette, drops the column centring, and picks A4 or Letter from the locale.
- **`Ctrl+P` prints through the preview**, so the math shows. It renders a temporary PDF and sends it to `QPrintDialog` through `QPdfDocument`, which adds `pdf` to the plugin's `QT`. Omawrite's `printDocument` stays untouched, so `Ctrl+P` needs an additive way to reach the new path.
- **If the preview is hidden or was never loaded**, print loads it offscreen first. That creates a second pane, so decide the profile lifetime (§6) first.
- The portal dialogs need a human to check them on Hyprland (SPEC §7).

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
