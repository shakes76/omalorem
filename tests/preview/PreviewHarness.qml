import QtQuick

// A bare window around the real PreviewPane, driven by tst_preview.
Window {
    id: harness
    width: 900
    height: 700
    visible: true

    property var lastResult
    property int resultSerial: 0
    readonly property alias pane: pane

    // runJavaScript is asynchronous; the test waits on resultSerial.
    function evaluate(script) {
        pane.view.runJavaScript(script, function(result) {
            harness.lastResult = result;
            harness.resultSerial++;
        });
    }

    PreviewPane {
        id: pane
        anchors.fill: parent
        bridge: testBridge
        profile: testSandbox.profile()
    }
}
