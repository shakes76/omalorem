#include <QFont>
#include <QFontDatabase>
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QUrl>
#include <QWindow>
#include <QFile>

#include "backend.h"
#include "systemtheme.h"

#ifndef OMAVIEW_NO_PREVIEW
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#endif

int main(int argc, char *argv[]) {
#ifndef OMAVIEW_NO_PREVIEW
    // QtWebEngine arrives later with the preview plugin, and needs OpenGL
    // context sharing set before the application exists. That is all
    // QtWebEngineQuick::initialize() does for us, and calling it would link
    // WebEngine into the executable, costing ~90 ms on every launch.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("omaview"));
    app.setDesktopFileName(QStringLiteral("omaview"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("omaview")));

    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/iAWriterMonoS-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/iAWriterMonoS-Italic.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/iAWriterMonoS-Bold.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/iAWriterMonoS-BoldItalic.ttf"));
    app.setOrganizationName(QStringLiteral("Omacom"));
    app.setOrganizationDomain(QStringLiteral("omacom.io"));

    QQuickStyle::setStyle(QStringLiteral("Material"));

    Backend backend(&app);
    SystemTheme systemTheme(&app);
    backend.setDarkMode(systemTheme.darkMode());
    QObject::connect(&systemTheme, &SystemTheme::darkModeChanged, &backend,
                     &Backend::setDarkMode);

    // Carry the desktop's text scale into the default font, so the chrome that
    // inherits it (dialog titles, buttons) grows along with the writing area.
    const QFont interfaceFont(QStringLiteral("iA Writer Mono S"));
    const qreal basePointSize = interfaceFont.pointSizeF() > 0
        ? interfaceFont.pointSizeF()
        : app.font().pointSizeF();
    const auto applyInterfaceFont = [&app, interfaceFont, basePointSize](qreal textScale) {
        QFont scaled = interfaceFont;
        scaled.setPointSizeF(basePointSize * textScale);
        app.setFont(scaled);
    };
    applyInterfaceFont(systemTheme.textScale());

    backend.setTextScale(systemTheme.textScale());
    QObject::connect(&systemTheme, &SystemTheme::textScaleChanged, &backend,
                     [&backend, applyInterfaceFont](qreal textScale) {
        applyInterfaceFont(textScale);
        backend.setTextScale(textScale);
    });

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            qWarning().noquote() << warning.toString();
    });
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
#ifndef OMAVIEW_NO_PREVIEW
    // The Omaview.Preview plugin sits beside the executable in a build tree,
    // and in <prefix>/lib/omaview/qml when installed. Without it the editor
    // runs as plain Omawrite: no footer button, and Ctrl+E does nothing.
    const QDir appDir(QCoreApplication::applicationDirPath());
    for (const QString &importPath : {appDir.absolutePath(),
                                      appDir.absoluteFilePath(QStringLiteral("../lib/omaview/qml"))}) {
        if (QFileInfo::exists(importPath + QStringLiteral("/Omaview/Preview/qmldir"))) {
            engine.addImportPath(importPath);
            backend.setPreviewAvailable(true);
            break;
        }
    }
#endif

    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Could not load the Omaview interface; resource available:"
                    << QFile::exists(QStringLiteral(":/Main.qml"));
        return -1;
    }

    backend.setParentWindow(qobject_cast<QWindow *>(engine.rootObjects().constFirst()));

    const QStringList args = app.arguments();
    if (args.size() > 1 && !backend.modified())
        backend.open(QUrl::fromLocalFile(args.at(1)));

    return app.exec();
}
