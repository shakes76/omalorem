import QtQuick
import QtWebChannel
import QtWebEngine

// The rendered document. It knows nothing about the window it sits in, so the
// docked placement (M3) can reuse it as is.
Item {
    id: pane

    // The PreviewBridge the page renders, and the sandboxed profile to load it
    // with (PreviewSandbox.profile()).
    required property QtObject bridge
    required property WebEngineProfile profile
    readonly property url pageUrl: "qrc:/preview/index.html"
    readonly property alias view: webView

    WebChannel {
        id: channel
    }

    WebEngineView {
        id: webView
        objectName: "previewView"
        anchors.fill: parent
        // The page paints the theme background itself; transparent here
        // avoids a white flash before the first render.
        backgroundColor: "transparent"
        profile: pane.profile
        webChannel: channel
        settings.javascriptCanOpenWindows: false
        settings.localContentCanAccessFileUrls: true
        settings.localContentCanAccessRemoteUrls: false
        settings.focusOnNavigationEnabled: false

        property bool pageRequested: false

        // The only navigation ever allowed is the initial load of the page.
        // A link that slips past the page's click handler goes to the bridge,
        // which opens http(s) and mailto externally and drops everything else.
        onNavigationRequested: function(request) {
            if (!pageRequested && request.url.toString() === pane.pageUrl.toString()) {
                pageRequested = true;
                request.accept();
                return;
            }
            request.reject();
            if (request.navigationType === WebEngineNavigationRequest.LinkClickedNavigation)
                pane.bridge.openLink(request.url.toString());
        }

        onNewWindowRequested: function(request) {
            pane.bridge.openLink(request.requestedUrl.toString());
        }
    }

    // The channel must know the bridge before the page asks for it.
    Component.onCompleted: {
        channel.registerObject("bridge", pane.bridge);
        webView.url = pane.pageUrl;
    }
}
