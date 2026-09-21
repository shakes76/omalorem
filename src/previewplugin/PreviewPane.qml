import QtQuick
import QtWebChannel
import QtWebEngine

// The rendered document. It knows nothing about the window it sits in, so the
// docked placement (M3) can reuse it as is.
Item {
    id: pane

    // The PreviewBridge the page renders.
    required property QtObject bridge
    readonly property url pageUrl: "qrc:/preview/index.html"
    readonly property alias view: viewLoader.item
    // How many times the window asked for a fresh frame (for the tests).
    property int repaintRequests: 0

    // Asks the page for a fresh frame; see preview.js. Harmless if the view
    // is fine, so the window calls it whenever it may have been hidden.
    function repaint() {
        repaintRequests++;
        if (view)
            view.runJavaScript("window.omaloremPreview && window.omaloremPreview.repaint()");
    }

    WebChannel {
        id: channel
        // Property changes reach the page in batches; the default 50 ms
        // makes scroll sync visibly step. One frame is enough.
        propertyUpdateInterval: 16
    }

    // No storage name, so the profile is off the record: no cookies, cache
    // or history reach the disk. PreviewSandbox adds the request interceptor.
    WebEngineProfilePrototype {
        id: profilePrototype
        httpCacheType: WebEngineProfile.MemoryHttpCache
        persistentCookiesPolicy: WebEngineProfile.NoPersistentCookies
    }

    // The view is created only once the pane (and so the profile prototype)
    // is complete: the prototype has no profile to hand out before that, and a
    // view without one would fall back to Chromium's default profile.
    Loader {
        id: viewLoader
        anchors.fill: parent
        active: false
        sourceComponent: webViewComponent
    }

    Component {
        id: webViewComponent

        WebEngineView {
            id: webView
            objectName: "previewView"
            // The page paints the theme background itself; transparent here
            // avoids a white flash before the first render.
            backgroundColor: "transparent"
            profile: PreviewSandbox.protect(profilePrototype.instance(), pane.bridge)
            webChannel: channel
            settings.javascriptCanOpenWindows: false
            // Document images come through omalorem-doc:, never file:.
            settings.localContentCanAccessFileUrls: false
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

            Component.onCompleted: url = pane.pageUrl
        }
    }

    // The channel must know the bridge before the page asks for it.
    Component.onCompleted: {
        channel.registerObject("bridge", pane.bridge);
        viewLoader.active = true;
    }
}
