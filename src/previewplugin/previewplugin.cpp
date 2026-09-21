#include <QQmlEngine>
#include <QQmlExtensionPlugin>

#include "previewsandbox.h"

// The Omaview.Preview QML module. The editor imports it only when the preview
// is first shown, and loading this plugin is what brings in QtWebEngine: the
// omaview executable itself does not link it (docs/SPEC.md §5.3).
class OmaviewPreviewPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override {
        // The page and its QML live in this plugin's resources.
        Q_INIT_RESOURCE(previewplugin);
        qmlRegisterSingletonType<PreviewSandbox>(
            uri, 1, 0, "PreviewSandbox",
            [](QQmlEngine *, QJSEngine *) -> QObject * { return new PreviewSandbox; });
    }
};

#include "previewplugin.moc"
