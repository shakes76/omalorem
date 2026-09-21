import QtQuick
import QtQuick.Window

// The pop-out preview: its own top-level window so Hyprland tiles it beside
// the editor. It deliberately has no transientParent, because Hyprland floats
// transient windows.
//
// Main.qml's Qt.ApplicationShortcut shortcuts already fire while this window
// has focus. Repeating them here would make each one ambiguous, and Qt then
// fires neither, so this window declares none of its own.
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

    Connections {
        target: previewWindow.editorWindow

        // Closing the editor closes its preview, so the application can quit.
        function onVisibleChanged() {
            if (previewWindow.editorWindow.visible)
                return;
            previewWindow.closingWithEditor = true;
            previewWindow.close();
        }
    }

    PreviewPane {
        anchors.fill: parent
        bridge: backend.previewBridge
        profile: previewSandbox.profile()
    }
}
