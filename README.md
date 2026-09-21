# Omalorem

**Omawrite, with a placeholder math-enabled preview.**

Omalorem is a fork of [Omawrite](https://github.com/omacom-io/omawrite), the dead-simple,
distraction-free Markdown writer for Omarchy. It keeps Omawrite's editor exactly as it is and
adds one thing beside it: a live preview window that renders your Markdown, with LaTeX math
typeset by [KaTeX](https://katex.org). You can check your equations while you write, then
close the preview and it's Omawrite again.

It is built with Qt Quick and C++ and automatically follows system dark/light mode.

## Why "lorem"?

*Lorem ipsum* is the dummy text that typesetters and designers use to fill a page before
the real words arrive, so the layout can be judged on its own. It is scrambled Latin from
Cicero's *De finibus bonorum et malorum* (45 BC). "Lorem" is what's left of *dolorem*,
"pain", cut off mid-word, and the rest has drifted too far from Cicero to mean anything.

The name suits a project that is only a small extension of Omawrite, not an app of its
own. The preview is a stand-in for the finished page: it shows how the text and the math
will look, while the writing itself stays in Omawrite's plain editor.

Omalorem was first released as Omaview 0.1.0 (tag `omaview-v0.1.0`).

## What the preview adds

- **Its own window.** On Omarchy, Hyprland tiles it beside the editor, and the editor keeps
  focus. `Ctrl+E` shows or hides it. Closing the preview only hides it, and closing the
  editor closes both.
- **Markdown**, CommonMark plus GFM: tables, task lists, strikethrough, autolinks and fenced
  code.
- **Math** with `$…$` and `\(…\)` inline and `$$…$$` and `\[…\]` for display. KaTeX
  environments such as `align`, `cases` and `pmatrix` work inside display math. A bad
  expression is shown as its source, in the accent colour, with KaTeX's error as a tooltip.
  The rest of the document still renders. Dollar amounts like `$5 and $10` stay text.
- **Live updates.** The preview re-renders 120 ms after you stop typing, and only the
  blocks you changed are re-rendered and re-typeset.
- **Scroll sync.** The preview follows the editor as you scroll, until you scroll the
  preview yourself.
- **Typography to match.** The preview uses the editor's iA Writer Mono S, in the same
  column width. `Ctrl+Shift+T` switches the prose to the proportional iA Writer Quattro S.
  Omarchy theme colours, light and dark mode, and the desktop text size apply live.
- **Local images** from the document's folder and its subfolders. Remote images, images
  from elsewhere, and images in an unsaved document show a placeholder with their alt
  text.
- **Offline and sandboxed.** Everything the preview uses is compiled into the app. It
  never touches the network, raw HTML is shown as text, and links open in your browser.
- **Nothing costs anything until you use it.** The preview's web engine (QtWebEngine)
  loads only the first time you show the preview. If you keep the preview hidden,
  Omalorem starts as fast as Omawrite.

```markdown
The heat equation $\partial_t u = \alpha \nabla^2 u$ has the solution

$$
u(x, t) = \frac{1}{\sqrt{4\pi\alpha t}} \int_{-\infty}^{\infty} e^{-\frac{(x-y)^2}{4\alpha t}} f(y)\, dy
$$
```

> **Status:** 0.1.0. The preview window is done. Still to come: a docked split-view
> placement (`Ctrl+Shift+E`), math highlighting and insertion shortcuts in the editor, and
> PDF export and printing with the math rendered. Until then, `Ctrl+P` prints the Markdown
> source, as Omawrite does. See [`docs/SPEC.md`](docs/SPEC.md) for the specification and
> milestones, and [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) for the development status,
> code layout and project rules. Upstream Omawrite is tracked as the `upstream` remote.

The screenshots below show Omawrite's editor, which Omalorem keeps unchanged:

<img width="2948" height="3227" alt="screenshot-2026-06-23_15-24-08" src="https://github.com/user-attachments/assets/4e930c0d-edda-4046-b444-a59eff523329" />
<img width="2948" height="3227" alt="screenshot-2026-06-23_15-23-23" src="https://github.com/user-attachments/assets/8ced7c26-961b-4ded-b263-84403001a951" />


## Install

Build and install locally with `bin/install` (requires `makepkg`), or just build with
`bin/build` and run `build/omalorem`. Tests: `bin/test`.

`bin/install` builds the Arch package in `pkgbuild/` and installs it with pacman. You can
also build the package alone with `makepkg -f` in `pkgbuild/`, and install it with
`sudo pacman -U pkgbuild/omalorem-*.pkg.tar.zst`. Omalorem then appears in the Omarchy app
launcher. It installs beside Omawrite without clashing, with its own binary, desktop
entry, icon and settings.

`bin/build-no-preview` builds the plain editor without the preview or QtWebEngine.

## Shortcuts

- `Ctrl+S` saves. Unsaved documents use the XDG desktop portal file picker.
- `Ctrl+Shift+S` saves as.
- `Ctrl+O` opens a Markdown file through the portal picker.
- `Ctrl+P` opens the system print dialog.
- `Ctrl+E` shows or hides the preview window. Closing the preview window only hides it.
- `Ctrl+Shift+T` switches the preview's prose between iA Writer Mono S and the proportional iA Writer Quattro S. The editor keeps Mono.
- `F11` in the preview window fullscreens the preview, for reading.
- `Ctrl+N` opens a new Omalorem window.
- `Ctrl+Z`, `Ctrl+Shift+Z`, and `Ctrl+Y` handle undo and redo.
- `Super+F` toggles fullscreen. Qt maps this key as `Meta+F`.
- `Ctrl+F` searches the document. Use `Enter` or `Ctrl+G` for the next match and `Shift+Enter` for the previous match.
- `Ctrl+H` opens find and replace.
- `Ctrl+B`, `Ctrl+I`, and `Ctrl+K` insert bold, italic, and link Markdown.
- `Ctrl+?` shows the keyboard shortcut reference.

Every shortcut except the editing ones also works while the preview window has focus. Find
moves you back to the editor.

Unsaved drafts are recovered after an abnormal exit. Omalorem also watches open files
and warns before an external change can replace local work.

Text follows the desktop text size — `omarchy display text size`, or GNOME's
`text-scaling-factor` — and re-flows without a restart. The default of 12px leaves
Omalorem at the size it is designed around; larger and smaller sizes scale from there.

## Requirements

- Qt 6: `qt6-base`, `qt6-declarative`, `qt6-quickcontrols2`
- For the preview: `qt6-webengine` and `qt6-webchannel`
- `xdg-desktop-portal` and a portal backend

## Licences and credits

Omalorem is MIT licensed, like Omawrite; see `LICENSE`.

The iA Writer Mono font is bundled under the SIL Open Font License 1.1; see
`fonts/OFL.txt`. The font is copyright Information Architects Inc. and based on
IBM Plex, copyright IBM Corp.
The preview also bundles iA Writer Quattro S under the same licence.

The preview bundles [markdown-it](https://github.com/markdown-it/markdown-it) (MIT),
[markdown-it-texmath](https://github.com/goessner/markdown-it-texmath) (MIT),
[KaTeX](https://github.com/KaTeX/KaTeX) (MIT) and Qt's `qwebchannel.js` (LGPL-3.0). The
exact versions, checksums and licence files are in `third_party/`.
