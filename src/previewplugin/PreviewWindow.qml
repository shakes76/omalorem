import QtQuick
import QtQuick.Window

// The pop-out preview: its own top-level window so Hyprland tiles it beside
// the editor. It deliberately has no transientParent, because Hyprland floats
// transient windows.
//
// Main.qml's Qt.ApplicationShortcut shortcuts already fire while this window
// has focus. Repeating them here would make each one ambiguous, and Qt then
// fires neither, so this window declares none of its own. The two it needs to
// behave differently here, find and F11, are handled below without touching
// the editor's handlers.
Window {
    id: previewWindow

    required property Window editorWindow
    property bool closingWithEditor: false

    // A Window declared inside another window's items would otherwise get
    // that window as its transient parent automatically.
    transientParent: null
    title: "Preview — " + backend.fileName + " - Omaview"
    color: backend.themeBackground
    visible: backend.previewVisible

    // A starting size only: a tiling compositor decides the real one, and
    // Omaview never saves it.
    Component.onCompleted: {
        width = editorWindow.width;
        height = editorWindow.height;
    }

    // Closing the preview (Super+W) only hides it; the document is unaffected.
    onClosing: function(close) {
        if (closingWithEditor)
            return;
        close.accepted = false;
        backend.previewVisible = false;
    }

    // The editor keeps keyboard focus when the preview appears. Compositors
    // may refuse the request; Hyprland honours it with misc:focus_on_activate,
    // which Omarchy enables.
    onVisibleChanged: {
        if (visible)
            Qt.callLater(editorWindow.requestActivate);
    }

    function toggleFullScreen() {
        visibility = visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen;
    }

    Connections {
        target: previewWindow.editorWindow

        // Closing the editor closes its preview, so the application can quit.
        function onVisibleChanged() {
            if (previewWindow.editorWindow.visible)
                return;
            previewWindow.closingWithEditor = true;
            previewWindow.close();
        }

        // Ctrl+F or Ctrl+H pressed here opens the editor's find bar; typing
        // belongs over there, so hand the keyboard to the editor window.
        function onSearchOpenChanged() {
            if (previewWindow.editorWindow.searchOpen && previewWindow.active)
                previewWindow.editorWindow.requestActivate();
        }
    }

    Item {
        anchors.fill: parent
        focus: true

        // F11 here fullscreens the preview, for reading. Main.qml binds F11
        // application-wide to the editor. Qt offers every shortcut to the
        // focused item first, as a ShortcutOverride that bubbles up to here
        // even from inside the web view; accepting it stops Main's shortcut.
        // The toggle happens here too, because once Chromium has focus the
        // key press itself never comes back out of the page.
        Keys.onShortcutOverride: function(event) {
            if (event.key !== Qt.Key_F11 || event.modifiers !== Qt.NoModifier)
                return;
            event.accepted = true;
            if (!event.isAutoRepeat)
                previewWindow.toggleFullScreen();
        }
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_F11 && event.modifiers === Qt.NoModifier)
                event.accepted = true;
        }

        PreviewPane {
            anchors.fill: parent
            bridge: backend.previewBridge
        }
    }
}
