import QtQuick
import QtQuick.Window
import QtWebEngine

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
    title: "Preview — " + backend.fileName + " - Omalorem"
    color: backend.themeBackground
    visible: backend.previewVisible

    // A starting size only: a tiling compositor decides the real one, and
    // Omalorem never saves it.
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

    // Coming back to a workspace can leave the web view black until the
    // page paints again (the compositor stops the hidden window, and Qt
    // drops the last frame). Ask for a fresh frame whenever the window may
    // have been hidden: when it is exposed again, and when either window
    // becomes active, which is where focus lands on returning.
    PreviewExposeWatcher {
        window: previewWindow
        onReexposed: previewWindow.repaint()
    }

    onActiveChanged: {
        if (active)
            repaint();
    }

    function repaint() {
        pane.repaint();
        repaintRetry.restart();
    }

    // Once more a moment later, in case Chromium was still being told the
    // window is visible again when the first request arrived.
    Timer {
        id: repaintRetry
        interval: 150
        onTriggered: pane.repaint()
    }

    // --- PDF export and print (docs/SPEC.md §5.5) ---------------------------
    // Both render the page to a PDF: export writes it where the user chose,
    // print sends it to a printer. Print asks the desktop's print portal
    // first: its dialog opens at once, the page is rendered on the paper
    // chosen there, and the portal prints the PDF as it is. Without the
    // portal, the PDF is rendered first and Qt's print dialog prints it.
    // This works while the window is hidden too; Main.qml loads it for that
    // and it stays hidden.
    property var outputRequest: null
    // Page size from the locale: Letter where the US system is used, else A4.
    readonly property bool letterPaper: Qt.locale().measurementSystem === Locale.ImperialUSSystem

    function produce(request) {
        if (outputRequest) {
            backend.previewReportStatus("Still preparing the last PDF.");
            return;
        }
        if (request.kind === "print" && PreviewSandbox.printTestTarget === ""
                && PreviewSandbox.portalPrintAvailable()) {
            // No timeout while the dialog is open: the user takes their time.
            outputRequest = { kind: "print", portal: true, preparing: true, started: false };
            PreviewSandbox.preparePortalPrint("Print " + backend.fileName);
            return;
        }
        startRender(request.kind, request.kind === "pdf" ? PreviewSandbox.localPath(request.url)
                                                          : PreviewSandbox.temporaryPdfPath(), null);
    }

    // Renders the page to `path` once it is current; `paper` is the portal's
    // choice, or null for the locale's A4 or Letter in portrait.
    function startRender(kind, path, paper) {
        if (path === "") {
            outputRequest = null;
            backend.previewReportStatus("Could not choose a file for the PDF.");
            return;
        }
        const portal = outputRequest && outputRequest.portal;
        const token = portal ? outputRequest.token : 0;
        outputRequest = { kind: kind, path: path, paper: paper, portal: portal, token: token,
                          started: false };
        outputTimeout.restart();
        backend.previewFlushMarkdown();
        Qt.callLater(continueOutput);
    }

    // Starts once the page shows the current text with fonts and images
    // loaded; the bridge's revisions say when.
    function continueOutput() {
        const request = outputRequest;
        const bridge = backend.previewBridge;
        if (!request || request.started || request.preparing || !pane.view || !bridge.pageReady
                || bridge.renderedRevision !== bridge.markdownRevision)
            return;
        request.started = true;
        const paper = request.paper || { sizeId: letterPaper ? WebEngineView.Letter : WebEngineView.A4,
                                         widthMm: letterPaper ? 215.9 : 210, landscape: false };
        pane.view.runJavaScript("window.omaloremPreview.preparePrint(" + paper.widthMm + ")", function() {
            pane.view.printToPdf(request.path, paper.sizeId,
                                 paper.landscape ? WebEngineView.Landscape : WebEngineView.Portrait);
        });
    }

    function finishOutput(filePath, success) {
        const request = outputRequest;
        outputRequest = null;
        outputTimeout.stop();
        if (!request)
            return;
        if (request.kind === "pdf") {
            backend.previewReportStatus(success ? "Exported " + filePath.split("/").pop()
                                                : "Could not export the PDF.");
            return;
        }
        if (!success) {
            backend.previewReportStatus("Could not prepare the document for printing.");
            PreviewSandbox.removeTemporaryPdf(filePath);
        } else if (request.portal) {
            // The portal removes nothing; portalPrint deletes the file after.
            PreviewSandbox.portalPrint(filePath, request.token, "Print " + backend.fileName);
        } else {
            if (PreviewSandbox.printPdf(filePath, "Print " + backend.fileName, editorWindow))
                backend.previewReportStatus("Sent " + backend.fileName + " to the printer");
            PreviewSandbox.removeTemporaryPdf(filePath);
        }
    }

    Connections {
        target: PreviewSandbox

        // The portal's dialog closed: render on the chosen paper, or stop.
        function onPortalPrintPrepared(response, token, paper) {
            const request = previewWindow.outputRequest;
            if (!request || !request.preparing)
                return;
            if (response !== 0) {
                previewWindow.outputRequest = null;
                backend.previewReportStatus(response === 1 ? "Print cancelled"
                                                           : "Could not open the print dialog.");
                return;
            }
            request.preparing = false;
            request.token = token;
            previewWindow.startRender("print", PreviewSandbox.temporaryPdfPath(), paper);
        }

        function onPortalPrintFinished(response) {
            backend.previewReportStatus(response === 0 ? "Sent " + backend.fileName + " to the printer"
                                      : response === 1 ? "Print cancelled"
                                                       : "Could not print " + backend.fileName);
        }
    }

    Connections {
        target: backend.previewBridge

        function onRenderedRevisionChanged() {
            previewWindow.continueOutput();
        }
        function onPageReadyChanged() {
            previewWindow.continueOutput();
        }
    }

    Connections {
        target: pane.view
        ignoreUnknownSignals: true

        function onPdfPrintingFinished(filePath, success) {
            previewWindow.finishOutput(filePath, success);
        }
    }

    // A page that never settles must not block the next request for good.
    Timer {
        id: outputTimeout
        interval: 30000
        onTriggered: {
            if (!previewWindow.outputRequest)
                return;
            previewWindow.outputRequest = null;
            backend.previewReportStatus("Could not prepare the PDF: the preview did not finish rendering.");
        }
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
        function onActiveChanged() {
            if (previewWindow.editorWindow.active && previewWindow.visible)
                previewWindow.repaint();
        }

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
            id: pane
            objectName: "previewPane"
            anchors.fill: parent
            bridge: backend.previewBridge
        }
    }
}
