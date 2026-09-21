# Omaview — Development status

Last updated: 2026-09-21 · Branch: `m1-preview-window` (not yet merged to `main`, not pushed)

This is the working log for developers and coding agents. It records where the project
stands, how the code is laid out, the rules every change must follow, and what was done
and learned in each milestone. **Read `docs/SPEC.md` first.** The spec says what Omaview
should be; this file says where the code is now. When they disagree, the spec wins, and
the gap should be fixed or raised.

## 1. Current state at a glance

| Milestone | Status | Notes |
|---|---|---|
| M0 Fork and rename | Done | Upstream Omawrite `8f98892`, fork commit `eb6bd1a` |
| M1 Pop-out preview window | Done | Commits `b908f24`…`96906ed` |
| M1.1 Lazy WebEngine, additive-only upstream diff | Done | Commits `fe28c62`, `22db1c8` |
| M2 Fonts, incremental render, scroll sync, images | **Next** | See §7 |
| M3 Docked placement | Not started | Profile lifetime needs a decision first (§6) |
| M4 Editor math support | Not started | |
| M5 PDF export and print | Not started | |
| M6 Packaging and Omarchy docs | Not started | PKGBUILD steps are already written in SPEC §6 |

What works today:
- The preview opens as its own top-level window with no transient parent, so Hyprland tiles it beside the editor.
- `Ctrl+E` and the footer preview button show and hide it, and the setting is saved in `preview/visible`.
- Markdown (CommonMark and GFM tables, strikethrough, autolinks, task lists) and KaTeX math (`$`, `$$`, `\(…\)`, `\[…\]`) render from qrc, with no network access.
- Theme colours, light and dark mode, and the desktop text scale apply live.
- The preview re-renders 120 ms after the last keystroke. Open, reload and recovery render at once.
- Closing the preview only hides it. Closing the editor closes both.
- `Ctrl+F` and `Ctrl+H` from the preview bring the editor forward. F11 in the preview fullscreens the preview.
- With the preview hidden, the `omaview` binary never loads QtWebEngine: `ldd` shows no WebEngine library, and none is mapped at runtime.
- `no_preview` builds are plain Omawrite with the Omaview name.

Tests: `bin/test` passes both targets. `tst_omaview` has 20 passed. `tst_preview` has 20 passed, and 1 expected failure for local images, which is M2 work.

## 2. Rules for every change

These come from the spec and from decisions made with the project owner. Don't break them without a spec change.

1. **Additive-only upstream diff (SPEC §2, principle 1).** Files that came from upstream Omawrite may only gain lines. No existing upstream line is modified or deleted. The only exception is the `Ctrl+?` reference text in `src/Main.qml`. Preview behaviour lives in the preview's own files and reaches the editor through additive hooks: new properties, functions, blocks and signals. The reason is to keep upstream rebases clean and keep the preview portable to another editor. Check with the audit in §5.
2. **Nothing costs anything until it's used.** WebEngine code belongs only in the `Omaview.Preview` plugin (`src/previewplugin/`), never in the executable. Don't call `QtWebEngineQuick::initialize()` from `main.cpp`: that links WebEngine and cost +26% cold start in M1.
3. **Offline and sandboxed.** All web assets are vendored in `third_party/` and compiled in through qrc. Follow the update procedure in `third_party/VERSIONS`, and never commit `node_modules` or `package.json`. The request interceptor allows only `qrc:`, plus the document-folder scheme once M2 adds it.
4. **Match the code style.** Use `QStringLiteral`, `Q_PROPERTY` with NOTIFY, the `m_` prefix, 4-space indents and C++17. In QML, sizes go through `win.scaledSize()` and colours come from `backend.theme*`. Comments explain *why*.
5. **Every behaviour change gets a QtTest case.** Keep all tests green, and report real test output.
6. **Commit one phase at a time** on a milestone branch. Each commit ends with the `Co-Authored-By` trailer. Don't push without being asked.
7. **Don't drive the user's live desktop** from an agent: no `wtype` or other key injection, no moving windows between workspaces, no screenshot tools. In M1 a test script sent keystrokes to the wrong window. Test offscreen, and list what a human needs to check on Hyprland. Never change Hyprland or Omarchy config permanently.

## 3. Code layout

```
src/
  main.cpp, Main.qml, backend.*, …    upstream editor (additive changes only)
  previewbridge.{h,cpp}               PreviewBridge: plain QObject, no WebEngine dependency
  previewpolicy.h                     header-only URL policy, shared by the bridge, plugin and tests
  PreviewHost.qml, previewhost.qrc    `import Omaview.Preview; PreviewWindow {}`; this import loads the plugin
  preview/                            index.html, preview.css, preview.js (the page)
  previewplugin/                      Omaview.Preview QML plugin; everything that needs QtWebEngine
    previewplugin.pro, qmldir, previewplugin.cpp, previewplugin.qrc
    previewsandbox.{h,cpp}            PreviewSandbox singleton and request interceptor
    PreviewPane.qml                   WebEngineView wrapper (profile prototype, navigation blocking)
    PreviewWindow.qml                 top-level window: title, close-to-hide, find focus, F11
third_party/                          markdown-it 14.3.2, markdown-it-texmath 1.0.0, katex 0.16.47,
                                      qwebchannel.js (Qt 6.11.2); VERSIONS has the pins and sha256s
tests/
  tst_omaview.cpp, tests.pro          editor, backend and bridge tests (no WebEngine)
  preview/                            tst_preview: real WebEngineView offscreen, plus the integration tests
  fixtures/                           sample, math-valid, math-invalid, currency, code, raw-html, remote .md, pixel.png
bin/
  build, test, install                upstream scripts; bin/test has appended lines for the preview tests
  build-no-preview                    builds the no_preview variant into build-no-preview/
```

### How the pieces connect

```
Main.qml TextEdit ──textChanged──▶ backend.previewEditorTextChanged()   (additive hook)
Backend ──▶ PreviewBridge (120 ms debounce; open/reload/recovery render at once)
Backend's theme, darkMode, textScale and fileUrl signals ──▶ bridge properties
Main.qml Loader (after the editor's first frame, while previewVisible)
   └─▶ PreviewHost.qml ──import──▶ Omaview.Preview plugin (loads WebEngine now)
          └─▶ PreviewWindow ─▶ PreviewPane ─▶ WebEngineView ─QWebChannel─▶ qrc:/preview/index.html
                                                                           markdown-it + texmath → KaTeX
```

- `main.cpp` sets `Qt::AA_ShareOpenGLContexts` before `QApplication`. It adds the plugin's import path, which is `build/` first, then `<bindir>/../lib/omaview/qml`, and calls `setPreviewAvailable(true)` only if the plugin's `qmldir` exists. Without the plugin, the app runs as plain Omawrite.
- `PreviewPane` builds an off-the-record profile from a `WebEngineProfilePrototype` with no storage name, and `PreviewSandbox.protect()` attaches the interceptor. The `WebEngineView` is created by a `Loader` only after the pane completes, because the prototype returns a null profile before then, and `setProfile(nullptr)` segfaults.
- Find from the preview: `PreviewWindow` watches the editor's `searchOpen` and calls `editorWindow.requestActivate()`.
- F11 in the preview: a focus wrapper accepts F11's `ShortcutOverride`, which bubbles up even out of Chromium, and toggles fullscreen in that same handler. The key press itself never comes back out of the web view.
- The page replaces all of `#content` on each render and keeps the scroll position. Top-level blocks already carry `data-source-line` for M2's scroll sync.
- Debug logging: `QT_LOGGING_RULES="omaview.preview.info=true"` logs the first render. `omaview.preview.sandbox` logs blocked requests.

## 4. Build, run and test

```sh
bin/build                         # build/omaview plus the plugin in build/Omaview/Preview/
build/omaview tests/fixtures/sample.md
bin/build-no-preview              # build-no-preview/omaview, with no WebEngine and no plugin
bin/test                          # tst_omaview, then tst_preview (offscreen)
```

- `tst_preview` needs no Chromium flags on a machine with working GL. Where GL is missing, set `QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu"`.
- Harmless noise in the test output: "Binding loop … Dialog" warnings, which come from upstream's `Main.qml`, and one "Fontconfig error" line during the interceptor test.

## 5. Checks worth repeating before each merge

```sh
# The executable must not link WebEngine
ldd build/omaview | grep -iE 'webengine|webchannel'          # expect no output

# Additive-only audit against the fork commit
UP=$(git ls-tree -r --name-only eb6bd1a | grep -vE '^(docs/|third_party/)')
git diff eb6bd1a HEAD -- $UP | grep -E '^-[^-]'              # expect only the old Ctrl+? text line
```

Timing method: offscreen, isolated settings, launch to the editor's first frame, median of 10–15 runs. Results at M1.1:

| Case | Result | Target |
|---|---|---|
| Preview hidden, against `no_preview` | about 281 ms each (≈0%) | ≤10% — met |
| Preview first render after the editor's frame | 732 ms (offscreen, no GPU) | ≤600 ms — open (SPEC §10 Q3) |

## 6. Known issues and open questions

- **First-render time** (SPEC §10 Q3). This needs measuring on Hyprland with the GPU. If it still misses, relax the target. Don't warm WebEngine up on a background thread, because that isn't safe.
- **Local images** don't render yet. Chromium refuses `file:` URLs to a `qrc:` page. M2 serves the document's folder through a custom URL scheme (`QWebEngineUrlSchemeHandler`) instead. That's the expected failure in `tst_preview`.
- **`Ctrl+F` from the preview while find is already open** leaves focus in the preview. This is accepted, to keep the editor's find handlers untouched.
- **Profile lifetime for M3.** The profile lives only as long as its `PreviewPane`. Before M3 recreates panes, decide whether it moves out, for example into `PreviewSandbox`.
- **Not yet checked by a human on Hyprland:**
  - fractional scaling at 1.25 and 1.5
  - the checklist in SPEC §7, repeated after each UI milestone

## 7. Next: M2

This is scope from SPEC §8 plus the decisions above:
- Mono and Quattro font switching (`Ctrl+Shift+T`), with Quattro S bundled in the plugin's qrc and saved as `preview/font`.
- Block-keyed DOM patching, meeting the budget of 16 ms per update on the 2,000-line fixture with 200 equations.
- Editor → preview scroll sync through `data-source-line` and `sourceLine`.
- The document-folder URL scheme for local images, which turns the expected failure into a pass. Also the §4.4 link and escaped-HTML behaviour.

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
| `22db1c8` | Created the `Omaview.Preview` plugin. The binary no longer links WebEngine. Added `WebEngineProfilePrototype`, which removed the Qt 6.11 deprecation warning, and a staged-install check. |

Result:
- Cold start with the preview hidden is now on par with `no_preview`.
- The upstream diff is 540 lines added and 1 line changed (the `Ctrl+?` text).
- The first render slowed to 732 ms offscreen, because WebEngine now loads on first show.

### Spec v0.5 (`a70fc5d`)
- Recorded the plugin file layout and the M6 packaging steps.
- Recorded the profile-lifetime note for M3 and the accepted `Ctrl+F` limit.
- Marked M1 and M1.1 done, and added the first-render target as an open question.
