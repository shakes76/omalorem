# Omalorem on Omarchy

Omalorem is built for Omarchy and other tiling window managers: the window manager
arranges the editor and the preview, and the app has no layout of its own
(`docs/SPEC.md` §3).

Nothing here is needed to use Omalorem, and **Omalorem never changes your Hyprland
configuration**. These are optional snippets to paste into your own config if you want
the behaviour they describe.

## What you get without configuring anything

- Both windows share the app id **`omalorem`**, from the desktop entry. Window rules tell
  them apart by **title**:
  - the editor: `notes.md - Omalorem`, with a leading `*` while there are unsaved changes;
  - the preview: `Preview — notes.md - Omalorem`, which always starts with `Preview — `
    (an em dash).
- Hyprland tiles the preview beside the editor, with your usual gaps, borders and
  opacity. Omalorem never saves or restores the preview's size or position.
- The editor keeps the keyboard when the preview opens. Omalorem asks for the editor to
  be activated, and Omarchy's default `misc.focus_on_activate = true` honours that.
- Print and file dialogs come from the desktop portal, and Omarchy already floats and
  centres portal windows (`xdg-desktop-portal-gtk`), so they don't disturb your tiling.
- `Super+W` on the preview only hides it; the document is untouched. `Ctrl+E` brings it
  back. Closing the editor closes both.

## Optional Hyprland snippets

Omarchy configures Hyprland in Lua. Put these in `~/.config/hypr/hyprland.lua`, or a
Lua file it requires, and keep your own backup first. After any change:

```sh
hyprctl reload
hyprctl configerrors     # expect "no errors"
```

**Keep focus in the editor, whatever the focus settings.** Only needed if you have
turned `misc.focus_on_activate` off, or your compositor ignores the request:

```lua
-- Don't hand the keyboard to Omalorem's preview when it opens.
o.window({ class = "^omalorem$", title = "^Preview — " }, { no_initial_focus = true })
```

**Float the preview instead of tiling it.** For a floating window of a fixed size,
rather than a tile beside the editor:

```lua
o.window({ class = "^omalorem$", title = "^Preview — " }, {
  float = true,
  size = { 900, 1100 },
  center = true,
})
```

**Keep the preview out of screen shares**, as Omarchy does for password managers:

```lua
o.window({ class = "^omalorem$", title = "^Preview — " }, { no_screen_share = true })
```

**Float the print and save dialogs** if you are not on Omarchy, which already does this:

```lua
o.window({ class = "^xdg-desktop-portal-gtk$" }, { float = true, center = true })
```

## Launcher and keybinding

Omalorem installs its own desktop entry and icon, so it appears in the Omarchy launcher
and in `omarchy menu` searches once the package is installed. Nothing to configure.

To add it to a menu of your own, extend
`~/.config/omarchy/extensions/omarchy-menu.jsonc`:

```jsonc
"personal.omalorem": { "icon": "󰈙", "label": "Omalorem", "action": "uwsm-app -- omalorem" },
```

For a keybinding, in `~/.config/hypr/bindings.lua`:

```lua
o.bind("SUPER + SHIFT + O", "Omalorem", { launch = "omalorem" })
```

Check first whether the keys are already bound, with `omarchy menu keybindings --print`.
If they are, unbind them before binding again (`hl.unbind("SUPER + SHIFT + O")`), and
remember what you have replaced.

## Themes and text size

Omalorem follows Omarchy live, in both windows, with no restart:

```sh
omarchy theme set catppuccin       # colours and light/dark
omarchy display text size 16       # text size; 12 is the size Omalorem is designed around
```

The preview uses the same colours, the same text size and the same font as the editor.
`Ctrl+Shift+T` switches the preview's prose to iA Writer Quattro S, which is bundled.

## If the preview misbehaves

- **Black preview after switching workspaces.** Fixed in Omalorem; if it ever comes back,
  run `QT_LOGGING_RULES="omalorem.preview.expose.debug=true" omalorem notes.md` from a
  terminal, switch away and back, and report what it printed.
- **The preview doesn't open at all.** The preview is a plugin; check it was installed:
  `ls /usr/lib/omalorem/qml/Omalorem/Preview/`. Without it, Omalorem runs as plain
  Omawrite, with no preview button and `Ctrl+E` doing nothing.
- **Fractional scaling.** At 1.25 or 1.5 the editor switches to Qt's scalable text
  rendering; if text looks wrong in either window, say so in an issue with your
  `hyprctl monitors all` output.
