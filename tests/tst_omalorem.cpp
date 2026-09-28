#include <QtTest>
#include <QFont>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QJSEngine>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include "backend.h"
#include "markdownhighlighter.h"
#include "previewbridge.h"
#include "previewpolicy.h"

class OmaloremTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
    }

    void countsWords() {
        QCOMPARE(Backend::countWords(QStringLiteral("one two-three don't 42")), 4);
        QCOMPARE(Backend::countWords(QStringLiteral("你好 世界")), 2);
        QCOMPARE(Backend::countWords(QString()), 0);
    }

    void normalizesLinks() {
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("www.example.com/path")),
                 QStringLiteral("https://www.example.com/path"));
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("mailto:writer@example.com")),
                 QStringLiteral("mailto:writer@example.com"));
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("example.com")).isEmpty());
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("file:///tmp/private")).isEmpty());
    }

    void suggestsSafeNames() {
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("My first draft\nBody")),
                 QStringLiteral("My first draft.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("A/B")), QStringLiteral("A-B.md"));
        QCOMPARE(Backend::suggestedFileName(QString()), QStringLiteral("Untitled.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("Already.md")),
                 QStringLiteral("Already.md"));
    }

    void findsInlineMarkdownRanges() {
        const auto markup = MarkdownHighlighter::inlineMarkup(
            QStringLiteral("**bold** and *italic* and [site](https://example.com)"));
        QCOMPARE(markup.size(), 3);
        QCOMPARE(markup.at(0).content.start, 2);
        QCOMPARE(markup.at(0).content.length, 4);
        QCOMPARE(markup.at(2).content.length, 4);
        QCOMPARE(markup.at(2).markers[0].length, 1);
    }

    void loadsCurrentOmarchyTheme() {
        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());

        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        const QString themeDirectory = homeDirectory.path()
            + QStringLiteral("/.local/state/omarchy/current/theme");
        QVERIFY(QDir().mkpath(themeDirectory));

        QFile colorsFile(themeDirectory + QStringLiteral("/colors.toml"));
        QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray palette(
            "mode = \"light\"\n"
            "accent = \"#112233\"\n"
            "selection = \"#445566\"\n"
            "background = \"#fefefe\"\n"
            "foreground = \"#101010\"\n");
        QCOMPARE(colorsFile.write(palette), qint64(palette.size()));
        colorsFile.close();

        Backend backend;
        QCOMPARE(backend.themeBackground(), QStringLiteral("#fefefe"));
        QCOMPARE(backend.themeForeground(), QStringLiteral("#101010"));
        QCOMPARE(backend.themeAccent(), QStringLiteral("#112233"));
        QCOMPARE(backend.themeSelection(), QStringLiteral("#445566"));
        QVERIFY(!backend.darkMode());
    }

    void ignoresFileWatcherEventsForSavedContents() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = directory.filePath(QStringLiteral("first-save.md"));
        Backend backend;
        QSignalSpy externalChangeSpy(&backend, &Backend::externalChangeDetected);

        backend.saveAs(QUrl::fromLocalFile(path));
        QVERIFY(QFileInfo::exists(path));

        QFile sameContents(path);
        QVERIFY(sameContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        sameContents.close();
        QTest::qWait(100);
        QCOMPARE(externalChangeSpy.count(), 0);

        QFile changedContents(path);
        QVERIFY(changedContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(changedContents.write("changed elsewhere"), qint64(17));
        changedContents.close();
        QTRY_COMPARE(externalChangeSpy.count(), 1);
    }

    void keepsCursorAndSelectionStableAcrossInsertions() {
        const QString mutationsPath = QFINDTESTDATA("../src/EditorMutations.js");
        QVERIFY(!mutationsPath.isEmpty());

        QQmlEngine engine;
        QQmlComponent component(&engine);
        const QByteArray harness = R"QML(
            import QtQuick
            import "EditorMutations.js" as EditorMutations

            TextEdit {
                property string insertionText
                property int insertionCursor
                property string wrappedText
                property int wrappedSelectionStart
                property int wrappedSelectionEnd

                Component.onCompleted: {
                    text = "alpha omega";
                    cursorPosition = 5;
                    EditorMutations.replaceRange(this, 5, 5, "one\r\ntwo");
                    insertionText = text;
                    insertionCursor = cursorPosition;

                    text = "alpha beta omega";
                    select(6, 10);
                    EditorMutations.replaceRange(this, selectionStart, selectionEnd,
                                                 "**beta**", 2, 6);
                    wrappedText = text;
                    wrappedSelectionStart = selectionStart;
                    wrappedSelectionEnd = selectionEnd;
                }
            }
        )QML";
        const QUrl harnessUrl = QUrl::fromLocalFile(
            QFileInfo(mutationsPath).absolutePath() + QStringLiteral("/MutationHarness.qml"));
        component.setData(harness, harnessUrl);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> editor(component.create());
        QVERIFY2(editor, qPrintable(component.errorString()));

        QCOMPARE(editor->property("insertionText").toString(),
                 QStringLiteral("alphaone\ntwo omega"));
        QCOMPARE(editor->property("insertionCursor").toInt(), 12);
        QCOMPARE(editor->property("wrappedText").toString(),
                 QStringLiteral("alpha **beta** omega"));
        QCOMPARE(editor->property("wrappedSelectionStart").toInt(), 8);
        QCOMPARE(editor->property("wrappedSelectionEnd").toInt(), 12);
    }

    void savesAndOpensFromFooterButtons() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QVERIFY(window->findChild<QObject *>(QStringLiteral("sourceEditor")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("renderedPreview")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("modeToggle")));

        QObject *saveButton = window->findChild<QObject *>(QStringLiteral("saveButton"));
        QObject *openButton = window->findChild<QObject *>(QStringLiteral("openButton"));
        QVERIFY(saveButton);
        QVERIFY(openButton);

        QSignalSpy saveDialogSpy(&backend, &Backend::saveDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(saveButton, "clicked"));
        QCOMPARE(saveDialogSpy.count(), 1);

        QSignalSpy openDialogSpy(&backend, &Backend::openDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(openButton, "clicked"));
        QCOMPARE(openDialogSpy.count(), 1);
    }

    void hidesPreviewControlsWithoutWebEngine() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        // This target, like a no_preview build, never marks the preview
        // available: the button is hidden and toggling is a no-op.
        QObject *previewButton = window->findChild<QObject *>(QStringLiteral("previewButton"));
        QVERIFY(previewButton);
        QCOMPARE(previewButton->property("visible").toBool(), false);
        const bool visibleBefore = backend.previewVisible();
        QVERIFY(QMetaObject::invokeMethod(previewButton, "clicked"));
        QObject *integration = window->findChild<QObject *>(QStringLiteral("previewIntegration"));
        QVERIFY(integration);
        QVERIFY(QMetaObject::invokeMethod(integration, "toggle"));
        QCOMPARE(backend.previewVisible(), visibleBefore);
    }

    void scalesTextWithDesktopTextSize() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 20);

        // `omarchy display text size 16` sets the GNOME factor to 16/12.
        backend.setTextScale(16.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 27);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 27);

        backend.setTextScale(9.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 15);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 15);
    }

    void remembersLastSaveDirectory() {
        QTemporaryDir saveDirectory;
        QVERIFY(saveDirectory.isValid());

        const QString savedPath = saveDirectory.filePath(QStringLiteral("first.md"));
        Backend savedDocument;
        savedDocument.saveAs(QUrl::fromLocalFile(savedPath));

        Backend nextDocument;
        QSignalSpy saveDialogSpy(&nextDocument, &Backend::saveDialogRequested);
        nextDocument.saveAsDialog();
        QCOMPARE(saveDialogSpy.count(), 1);

        const QUrl suggestedUrl = saveDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).absolutePath(),
                 saveDirectory.path());
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).fileName(),
                 QStringLiteral("Untitled.md"));

        QSettings().setValue(QStringLiteral("file/lastSaveDirectory"),
                             saveDirectory.filePath(QStringLiteral("missing")));
        Backend fallbackDocument;
        QSignalSpy fallbackDialogSpy(&fallbackDocument, &Backend::saveDialogRequested);
        fallbackDocument.saveAsDialog();
        const QUrl fallbackUrl = fallbackDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(fallbackUrl.toLocalFile()).absolutePath(), QDir::homePath());
    }

    void debouncesPreviewMarkdown() {
        PreviewBridge bridge;
        QSignalSpy markdownSpy(&bridge, &PreviewBridge::markdownChanged);

        bridge.scheduleMarkdown(QStringLiteral("a"));
        bridge.scheduleMarkdown(QStringLiteral("ab"));
        bridge.scheduleMarkdown(QStringLiteral("abc"));
        QCOMPARE(markdownSpy.count(), 0);
        QTRY_COMPARE(markdownSpy.count(), 1);
        QCOMPARE(bridge.markdown(), QStringLiteral("abc"));

        // Opening a file renders at once and drops any keystrokes still pending.
        bridge.scheduleMarkdown(QStringLiteral("typed"));
        bridge.setMarkdown(QStringLiteral("opened"));
        QCOMPARE(markdownSpy.count(), 2);
        QTest::qWait(PreviewBridge::debounceInterval * 2);
        QCOMPARE(markdownSpy.count(), 2);
        QCOMPARE(bridge.markdown(), QStringLiteral("opened"));
    }

    void feedsEditorTextToPreview() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        // Available, but hidden: Main.qml's preview block feeds the bridge
        // without ever loading the preview window (and WebEngine).
        QSettings().setValue(QStringLiteral("preview/visible"), false);
        Backend backend;
        backend.setPreviewAvailable(true);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);

        PreviewBridge *bridge = backend.previewBridge();
        QVERIFY(bridge);
        QSignalSpy markdownSpy(bridge, &PreviewBridge::markdownChanged);

        editor->setProperty("text", QStringLiteral("$x^2$"));
        editor->setProperty("text", QStringLiteral("$x^3$"));
        QCOMPARE(markdownSpy.count(), 0);
        QTRY_COMPARE(markdownSpy.count(), 1);
        QCOMPARE(bridge->markdown(), QStringLiteral("$x^3$"));

        // Opening a file renders at once, without the debounce.
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("opened.md"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# Opened\n");
        file.close();
        backend.open(QUrl::fromLocalFile(path));
        QCOMPARE(markdownSpy.count(), 2);
        QCOMPARE(bridge->markdown(), QStringLiteral("# Opened\n"));
        QVERIFY(bridge->baseUrl().endsWith(QLatin1Char('/')));

        QSettings().remove(QStringLiteral("preview"));
    }

    void updatesPreviewThemeFromOmarchy() {
        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());

        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        const QString themeDirectory = homeDirectory.path()
            + QStringLiteral("/.local/state/omarchy/current/theme");
        QVERIFY(QDir().mkpath(themeDirectory));
        const auto writeColors = [&](const QByteArray &palette) {
            QFile colorsFile(themeDirectory + QStringLiteral("/colors.toml"));
            QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(colorsFile.write(palette), qint64(palette.size()));
        };
        writeColors("mode = \"dark\"\n"
                    "accent = \"#abcdef\"\n"
                    "selection = \"#123456\"\n"
                    "background = \"#0a0b0c\"\n"
                    "foreground = \"#f0f1f2\"\n");

        Backend backend;
        const QVariantMap dark = backend.previewBridge()->theme();
        QCOMPARE(dark.value(QStringLiteral("bg")).toString(), QStringLiteral("#0a0b0c"));
        QCOMPARE(dark.value(QStringLiteral("fg")).toString(), QStringLiteral("#f0f1f2"));
        QCOMPARE(dark.value(QStringLiteral("accent")).toString(), QStringLiteral("#abcdef"));
        QCOMPARE(dark.value(QStringLiteral("selection")).toString(), QStringLiteral("#123456"));
        QCOMPARE(dark.value(QStringLiteral("muted")).toString(), QStringLiteral("#909191"));
        QCOMPARE(dark.value(QStringLiteral("dark")).toBool(), true);

        QSignalSpy themeSpy(backend.previewBridge(), &PreviewBridge::themeChanged);
        writeColors("mode = \"light\"\n"
                    "accent = \"#112233\"\n"
                    "selection = \"#445566\"\n"
                    "background = \"#fefefe\"\n"
                    "foreground = \"#101010\"\n");
        QTRY_VERIFY(backend.previewBridge()->theme().value(QStringLiteral("bg"))
                    == QStringLiteral("#fefefe"));
        QVERIFY(themeSpy.count() >= 1);
        const QVariantMap light = backend.previewBridge()->theme();
        QCOMPARE(light.value(QStringLiteral("accent")).toString(), QStringLiteral("#112233"));
        QCOMPARE(light.value(QStringLiteral("muted")).toString(), QStringLiteral("#aeb1b5"));
        QCOMPARE(light.value(QStringLiteral("dark")).toBool(), false);

        backend.setTextScale(1.5);
        QCOMPARE(backend.previewBridge()->textScale(), 1.5);
    }

    void pointsPreviewAtDocumentFolder() {
        PreviewBridge bridge;
        QCOMPARE(bridge.baseUrl(), QString());

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("saved.md"));
        Backend backend;
        QCOMPARE(backend.previewBridge()->baseUrl(), QString());
        backend.saveAs(QUrl::fromLocalFile(path));
        const QString baseUrl = backend.previewBridge()->baseUrl();
        QVERIFY(baseUrl.startsWith(QStringLiteral("file:///")));
        QVERIFY(baseUrl.endsWith(QLatin1Char('/')));
        QCOMPARE(QUrl(baseUrl).toLocalFile(),
                 QDir(directory.path()).absolutePath() + QLatin1Char('/'));

        bridge.setDocumentUrl(QUrl::fromLocalFile(QStringLiteral("/tmp/a b/c.md")));
        QCOMPARE(bridge.baseUrl(), QStringLiteral("file:///tmp/a b/"));
        bridge.setDocumentUrl(QUrl());
        QCOMPARE(bridge.baseUrl(), QString());
    }

    void filtersPreviewLinks() {
        PreviewBridge bridge;
        QSignalSpy linkSpy(&bridge, &PreviewBridge::externalLinkRequested);

        bridge.openLink(QStringLiteral("javascript:alert(1)"));
        bridge.openLink(QStringLiteral("JavaScript:alert(1)"));
        bridge.openLink(QStringLiteral("file:///etc/passwd"));
        bridge.openLink(QStringLiteral("qrc:/preview/index.html"));
        bridge.openLink(QStringLiteral("data:text/html,<b>x</b>"));
        bridge.openLink(QStringLiteral("relative/page.md"));
        QCOMPARE(linkSpy.count(), 0);

        bridge.openLink(QStringLiteral("https://example.com/a"));
        bridge.openLink(QStringLiteral("http://example.com"));
        bridge.openLink(QStringLiteral("mailto:writer@example.com"));
        QCOMPARE(linkSpy.count(), 3);
        QCOMPARE(linkSpy.at(0).constFirst().toUrl(), QUrl(QStringLiteral("https://example.com/a")));
    }

    void allowsOnlyBundledAndDocumentResources() {
        PreviewBridge bridge;
        const QUrl image(QStringLiteral("omalorem-doc:/figures/plot.png"));
        QVERIFY(bridge.isResourceAllowed(QUrl(QStringLiteral("qrc:/preview/index.html"))));
        QVERIFY(!bridge.isResourceAllowed(image));

        bridge.setDocumentUrl(QUrl::fromLocalFile(QStringLiteral("/notes/heat.md")));
        QVERIFY(bridge.isResourceAllowed(image));
        QVERIFY(bridge.isResourceAllowed(QUrl(QStringLiteral("omalorem-doc:/a%20b.png"))));
        QVERIFY(bridge.isResourceAllowed(QUrl(QStringLiteral("OMALOREM-DOC:/x.png"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("omalorem-doc:/"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("omalorem-doc:/../etc/passwd"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("omalorem-doc:/%2e%2e/etc/passwd"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("omalorem-doc:/..%2Fetc/passwd"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("omalorem-doc://host/x.png"))));
        // Local files never load directly, not even the document's own.
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("file:///notes/figures/plot.png"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("file:///etc/passwd"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("https://example.com/x.png"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("data:image/png;base64,AAAA"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("javascript:alert(1)"))));
    }

    void mapsDocumentSchemeIntoFolder() {
        const QString base = QStringLiteral("file:///notes/a b/");
        QCOMPARE(previewDocumentPath(QUrl(QStringLiteral("omalorem-doc:/fig/plot.png")), base),
                 QStringLiteral("/notes/a b/fig/plot.png"));
        QCOMPARE(previewDocumentPath(QUrl(QStringLiteral("omalorem-doc:/x%20y.png")), base),
                 QStringLiteral("/notes/a b/x y.png"));
        QCOMPARE(previewDocumentPath(QUrl(QStringLiteral("omalorem-doc:/fig/../x.png")), base),
                 QStringLiteral("/notes/a b/x.png"));
        QCOMPARE(previewDocumentPath(QUrl(QStringLiteral("omalorem-doc:/../a b2/x.png")), base),
                 QString());
        QCOMPARE(previewDocumentPath(QUrl(QStringLiteral("omalorem-doc:/x.png")), QString()),
                 QString());
        QCOMPARE(previewDocumentPath(QUrl(QStringLiteral("file:///notes/a b/x.png")), base),
                 QString());
    }

    void storesPreviewSettings() {
        QSettings settings;
        settings.remove(QStringLiteral("preview"));

        {
            Backend backend;
            QCOMPARE(backend.previewVisible(), true);
            QCOMPARE(backend.previewPlacement(), QStringLiteral("window"));
            QCOMPARE(backend.previewAvailable(), false);
            QVERIFY(!settings.contains(QStringLiteral("preview/visible")));

            QSignalSpy visibleSpy(&backend, &Backend::previewVisibleChanged);
            backend.setPreviewVisible(false);
            backend.setPreviewVisible(false);
            QCOMPARE(visibleSpy.count(), 1);
            backend.setPreviewPlacement(QStringLiteral("docked"));
            backend.setPreviewPlacement(QStringLiteral("sideways"));
            QCOMPARE(backend.previewPlacement(), QStringLiteral("docked"));
        }

        settings.sync();
        QCOMPARE(settings.value(QStringLiteral("preview/visible")).toBool(), false);
        QCOMPARE(settings.value(QStringLiteral("preview/placement")).toString(),
                 QStringLiteral("docked"));
        {
            Backend backend;
            QCOMPARE(backend.previewVisible(), false);
            QCOMPARE(backend.previewPlacement(), QStringLiteral("docked"));
        }

        settings.setValue(QStringLiteral("preview/placement"), QStringLiteral("bogus"));
        {
            Backend backend;
            QCOMPARE(backend.previewPlacement(), QStringLiteral("window"));
        }
        settings.remove(QStringLiteral("preview"));
    }

    void switchesPreviewFont() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());
        QSettings settings;
        settings.remove(QStringLiteral("preview"));
        settings.setValue(QStringLiteral("preview/visible"), false);

        {
            Backend backend;
            QCOMPARE(backend.previewFont(), QStringLiteral("mono"));
            QCOMPARE(backend.previewBridge()->fontFamily(), QStringLiteral("mono"));
            backend.setPreviewAvailable(true);
            QQmlEngine engine;
            engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
            QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));
            QScopedPointer<QObject> root(component.create());
            auto *window = qobject_cast<QQuickWindow *>(root.data());
            QVERIFY(window);
            window->requestActivate();
            QVERIFY(QTest::qWaitForWindowActive(window));

            // Ctrl+Shift+T flips the preview's font and the bridge follows.
            QSignalSpy fontSpy(&backend, &Backend::previewFontChanged);
            QTest::keyClick(window, Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier);
            QCOMPARE(backend.previewFont(), QStringLiteral("quattro"));
            QCOMPARE(backend.previewBridge()->fontFamily(), QStringLiteral("quattro"));
            QTest::keyClick(window, Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier);
            QCOMPARE(backend.previewFont(), QStringLiteral("mono"));
            QCOMPARE(fontSpy.count(), 2);

            backend.setPreviewFont(QStringLiteral("quattro"));
            backend.setPreviewFont(QStringLiteral("comic"));
            QCOMPARE(backend.previewFont(), QStringLiteral("quattro"));
        }

        settings.sync();
        QCOMPARE(settings.value(QStringLiteral("preview/font")).toString(), QStringLiteral("quattro"));
        {
            Backend backend;
            QCOMPARE(backend.previewFont(), QStringLiteral("quattro"));
            QCOMPARE(backend.previewBridge()->fontFamily(), QStringLiteral("quattro"));
        }
        settings.setValue(QStringLiteral("preview/font"), QStringLiteral("bogus"));
        {
            Backend backend;
            QCOMPARE(backend.previewFont(), QStringLiteral("mono"));
        }
        settings.remove(QStringLiteral("preview"));
    }

    // --- Omalorem editor math (M4) ------------------------------------------

    void findsMathSpans() {
        using H = MarkdownHighlighter;
        const auto contents = [](const QString &text, int previous = -1, int *state = nullptr) {
            QStringList found;
            for (const H::MathSpan &span : H::mathSpans(text, previous, state))
                found.append(text.mid(span.content.start, span.content.length));
            return found;
        };

        QCOMPARE(contents(QStringLiteral("a $x$ b")), QStringList{QStringLiteral("x")});
        const QList<H::MathSpan> spans = H::mathSpans(QStringLiteral("a $x$ b"), -1);
        QCOMPARE(spans.at(0).markers[0].start, 2);
        QCOMPARE(spans.at(0).markers[1].start, 4);
        QCOMPARE(contents(QStringLiteral("$x_1 + y_2$ and $\\alpha$")),
                 QStringList({QStringLiteral("x_1 + y_2"), QStringLiteral("\\alpha")}));
        // The preview's rules: currency, escapes, code and digits stay text.
        QCOMPARE(contents(QStringLiteral("cost $5 and the book $10.")), QStringList());
        QCOMPARE(contents(QStringLiteral("between $5 and $10, or $20")), QStringList());
        QCOMPARE(contents(QStringLiteral("a \\$x$ b")), QStringList());
        QCOMPARE(contents(QStringLiteral("$ x $ and $x $")), QStringList());
        QCOMPARE(contents(QStringLiteral("$x$5")), QStringList());
        QCOMPARE(contents(QStringLiteral("`$x$` and ``a ` $b$`` then $y$")),
                 QStringList{QStringLiteral("y")});
        QCOMPARE(contents(QStringLiteral("\\(a + b\\) and \\[c\\] and $$E=mc^2$$")),
                 QStringList({QStringLiteral("a + b"), QStringLiteral("c"), QStringLiteral("E=mc^2")}));

        // Display math over several lines carries its state.
        int state = -1;
        QCOMPARE(contents(QStringLiteral("$$"), -1, &state), QStringList{QString()});
        QCOMPARE(state, int(H::MathDisplayDollars));
        QCOMPARE(contents(QStringLiteral("a_1 * b_2 * c"), state, &state),
                 QStringList{QStringLiteral("a_1 * b_2 * c")});
        QCOMPARE(state, int(H::MathDisplayDollars));
        QCOMPARE(contents(QStringLiteral("$$ then $y$"), state, &state),
                 QStringList({QString(), QStringLiteral("y")}));
        QCOMPARE(state, int(H::MathNormal));
        QCOMPARE(contents(QStringLiteral("\\[ x"), -1, &state), QStringList{QStringLiteral(" x")});
        QCOMPARE(state, int(H::MathDisplayBrackets));
        QCOMPARE(contents(QStringLiteral("y \\]"), state, &state), QStringList{QStringLiteral("y ")});
        QCOMPARE(state, int(H::MathNormal));

        // Fenced code holds no math, and closes only on its own character.
        QCOMPARE(contents(QStringLiteral("```"), -1, &state), QStringList());
        QCOMPARE(state, int(H::MathBacktickFence));
        QCOMPARE(contents(QStringLiteral("$x$ and $$"), state, &state), QStringList());
        QCOMPARE(contents(QStringLiteral("~~~"), state, &state), QStringList());
        QCOMPARE(state, int(H::MathBacktickFence));
        QCOMPARE(contents(QStringLiteral("```"), state, &state), QStringList());
        QCOMPARE(state, int(H::MathNormal));
        QCOMPARE(contents(QStringLiteral("$x$"), state, &state), QStringList{QStringLiteral("x")});
    }

    void highlightsMath() {
        QTextDocument document;
        // A bare document keeps no formats until it has a layout, as the
        // editor's always does.
        document.documentLayout();
        MarkdownHighlighter highlighter(&document);
        highlighter.setColors(QStringLiteral("#101010"), QStringLiteral("#eeeeee"),
                              QStringLiteral("#5584aa"));
        document.setPlainText(QStringLiteral(
            "Sum $x_1 + y_2$ and _it_\n"
            "$$\n"
            "a_1 b_2\n"
            "$$\n"
            "```\n"
            "$z$\n"
            "```"));

        const auto formatAt = [&](int line, int column) {
            const QTextBlock block = document.findBlockByNumber(line);
            for (const QTextLayout::FormatRange &range : block.layout()->formats()) {
                if (column >= range.start && column < range.start + range.length)
                    return range.format;
            }
            return QTextCharFormat();
        };
        QColor math(QStringLiteral("#5584aa"));
        math.setAlphaF(0.8);
        const auto isMath = [&](int line, int column) {
            return formatAt(line, column).foreground().color() == math;
        };
        const auto isHidden = [&](int line, int column) {
            return formatAt(line, column).fontLetterSpacing() < 0;
        };

        // Inline: math coloured, underscores shown, dollars muted.
        QVERIFY(isMath(0, 5));
        QVERIFY(isMath(0, 6));
        QVERIFY(!isHidden(0, 6));
        QVERIFY(!formatAt(0, 6).fontItalic());
        QVERIFY(!isMath(0, 4));
        QCOMPARE(formatAt(0, 4).foreground().color(), QColor(QStringLiteral("#4f525a")));
        // Emphasis outside math still hides its markers.
        QVERIFY(isHidden(0, 20));
        QVERIFY(formatAt(0, 21).fontItalic());
        // Display block: every line inside is math, underscores shown.
        QVERIFY(isMath(2, 0));
        QVERIFY(isMath(2, 1));
        QVERIFY(!isHidden(2, 1));
        QCOMPARE(document.findBlockByNumber(2).userState(), int(MarkdownHighlighter::MathDisplayDollars));
        // Fenced code: no math.
        QVERIFY(!isMath(5, 1));
    }

    void keepsCaretOnMathMarkers() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());
        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);

        // Only _it_'s markers are hidden; the formula's underscores are not.
        editor->setProperty("text", QStringLiteral("$x_1 + y_2$ _it_"));
        const QVariantList ranges = backend.hiddenRangesAt(0);
        QCOMPARE(ranges.size(), 2);
        QCOMPARE(ranges.at(0).toMap().value(QStringLiteral("start")).toInt(), 12);
        QCOMPARE(ranges.at(1).toMap().value(QStringLiteral("start")).toInt(), 15);

        // The same inside a display block, where the line alone looks plain.
        editor->setProperty("text", QStringLiteral("$$\na_1 b_2\n$$"));
        QCOMPARE(backend.hiddenRangesAt(4).size(), 0);
        QVERIFY(backend.displayMathOpenAt(4));
        QVERIFY(!backend.displayMathOpenAt(13));
    }

    void buildsMathEdits() {
        const QString path = QFINDTESTDATA("../src/EditorMath.js");
        QVERIFY(!path.isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString source = QString::fromUtf8(file.readAll());
        source.remove(QStringLiteral(".pragma library"));
        QJSEngine engine;
        const QJSValue loaded = engine.evaluate(source, path);
        QVERIFY2(!loaded.isError(), qPrintable(loaded.toString()));

        // Applies an edit and returns the text with the selection marked [ ].
        const auto run = [&](const char *function, const QString &text, int start, int end) {
            const QJSValue edit = engine.globalObject().property(QString::fromLatin1(function))
                .call({text, start, end});
            const int from = edit.property(QStringLiteral("start")).toInt();
            const QString replacement = edit.property(QStringLiteral("replacement")).toString();
            QString result = text.left(from) + replacement
                + text.mid(edit.property(QStringLiteral("end")).toInt());
            const int selectionEnd = from + edit.property(QStringLiteral("selectionEnd")).toInt();
            result.insert(selectionEnd, QLatin1Char(']'));
            result.insert(from + edit.property(QStringLiteral("selectionStart")).toInt(),
                          QLatin1Char('['));
            return result;
        };

        QCOMPARE(run("inlineMath", QString(), 0, 0), QStringLiteral("$[]$"));
        QCOMPARE(run("inlineMath", QStringLiteral("let x be"), 4, 5), QStringLiteral("let $[x]$ be"));
        QCOMPARE(run("inlineMath", QStringLiteral("let x be"), 3, 6), QStringLiteral("let $[x]$ be"));

        QCOMPARE(run("displayMath", QString(), 0, 0), QStringLiteral("$$\n[]\n$$"));
        QCOMPARE(run("displayMath", QStringLiteral("para"), 4, 4),
                 QStringLiteral("para\n\n$$\n[]\n$$"));
        QCOMPARE(run("displayMath", QStringLiteral("para\n\nnext"), 5, 5),
                 QStringLiteral("para\n\n$$\n[]\n$$\n\nnext"));
        QCOMPARE(run("displayMath", QStringLiteral("para\n\nnext"), 0, 0),
                 QStringLiteral("$$\n[]\n$$\n\npara\n\nnext"));
        QCOMPARE(run("displayMath", QStringLiteral("\npara"), 0, 0),
                 QStringLiteral("$$\n[]\n$$\n\npara"));
        QCOMPARE(run("displayMath", QStringLiteral("the y=x here"), 4, 7),
                 QStringLiteral("the\n\n$$\n[y=x]\n$$\n\nhere"));
        QCOMPARE(run("displayMath", QStringLiteral("a\n\nx^2\n\nb"), 3, 6),
                 QStringLiteral("a\n\n$$\n[x^2]\n$$\n\nb"));
    }

    void editsMathFromTheKeyboard() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());
        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> root(component.create());
        auto *window = qobject_cast<QQuickWindow *>(root.data());
        QVERIFY(window);
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));
        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        const auto select = [&](int start, int end) {
            QMetaObject::invokeMethod(editor, "select", Q_ARG(int, start), Q_ARG(int, end));
        };
        const auto type = [&](const char *text) {
            for (const char *c = text; *c; ++c)
                QTest::keyClick(window, *c);
        };

        // Ctrl+M wraps the selection and keeps it selected.
        editor->setProperty("text", QStringLiteral("let x be"));
        select(4, 5);
        QTest::keyClick(window, Qt::Key_M, Qt::ControlModifier);
        QCOMPARE(editor->property("text").toString(), QStringLiteral("let $x$ be"));
        QCOMPARE(editor->property("selectedText").toString(), QStringLiteral("x"));
        // One undo restores the text.
        QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(editor->property("text").toString(), QStringLiteral("let x be"));

        // Without a selection it leaves the caret between the dollars.
        editor->setProperty("text", QString());
        QTest::keyClick(window, Qt::Key_M, Qt::ControlModifier);
        QCOMPARE(editor->property("text").toString(), QStringLiteral("$$"));
        QCOMPARE(editor->property("cursorPosition").toInt(), 1);

        // Ctrl+Shift+M opens a display block on lines of its own, and Return
        // inside it adds one line, with no blank line or list marker.
        editor->setProperty("text", QStringLiteral("- item"));
        editor->setProperty("cursorPosition", 6);
        QTest::keyClick(window, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(editor->property("text").toString(), QStringLiteral("- item\n\n$$\n\n$$"));
        QCOMPARE(editor->property("cursorPosition").toInt(), 11);
        type("a = b");
        QTest::keyClick(window, Qt::Key_Return);
        type("- c");
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(editor->property("text").toString(),
                 QStringLiteral("- item\n\n$$\na = b\n- c\n\n$$"));

        // After the block closes, Return is the usual paragraph break.
        editor->setProperty("cursorPosition", editor->property("text").toString().length());
        QTest::keyClick(window, Qt::Key_Return);
        QVERIFY(editor->property("text").toString().endsWith(QStringLiteral("$$\n\n")));
    }

    // --- Omalorem PDF export and print (M5) ----------------------------------

    void tracksRenderedRevision() {
        PreviewBridge bridge;
        QSignalSpy renderedSpy(&bridge, &PreviewBridge::renderedRevisionChanged);
        const int start = bridge.markdownRevision();
        bridge.setMarkdown(QStringLiteral("a"));
        bridge.setMarkdown(QStringLiteral("a"));
        QCOMPARE(bridge.markdownRevision(), start + 1);
        QVERIFY(!bridge.renderedCurrent());
        bridge.rendered(bridge.markdownRevision());
        QCOMPARE(renderedSpy.count(), 1);
        // Not current until the page has said it is ready.
        QVERIFY(!bridge.renderedCurrent());
        bridge.ready();
        QVERIFY(bridge.renderedCurrent());
        bridge.setMarkdown(QStringLiteral("b"));
        QVERIFY(!bridge.renderedCurrent());
    }

    // Ctrl+P goes to the preview when there is one; the no_preview build
    // keeps Omawrite's print (which opens a dialog, so isn't run here).
    void routesPrintThroughPreview() {
        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nTextEdit { text: 'x'\n"
                          "  Component.onCompleted: backend.attachDocument(textDocument) }",
                          QUrl(QStringLiteral("qrc:/PrintHarness.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> editor(component.create());
        QVERIFY(editor);

        QSignalSpy printSpy(&backend, &Backend::previewPrintRequested);
        backend.setPreviewAvailable(true);
        backend.printDocument();
        QCOMPARE(printSpy.count(), 1);
    }

    void suggestsPdfBesideDocument() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Backend backend;
        QSignalSpy dialogSpy(&backend, &Backend::previewPdfDialogRequested);
        backend.previewExportPdfDialog();
        QCOMPARE(dialogSpy.count(), 0);

        backend.setPreviewAvailable(true);
        backend.saveAs(QUrl::fromLocalFile(directory.filePath(QStringLiteral("heat.notes.md"))));
        backend.previewExportPdfDialog();
        QCOMPARE(dialogSpy.count(), 1);
        QCOMPARE(dialogSpy.at(0).constFirst().toUrl().toLocalFile(),
                 directory.filePath(QStringLiteral("heat.notes.pdf")));
    }

    // --- Omalorem document links and history (M7) ----------------------------

    void followsOnlyMarkdownLinksInFolder() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QDir dir(root.path());
        QVERIFY(dir.mkpath(QStringLiteral("notes/sub")) && dir.mkpath(QStringLiteral("outside")));
        for (const QString &name : {QStringLiteral("notes/a.md"), QStringLiteral("notes/sub/b.md"),
                                    QStringLiteral("notes/upper.MD"), QStringLiteral("notes/plain.txt"),
                                    QStringLiteral("outside/other.md")}) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("# heading\n");
        }
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("outside/other.md")),
                            dir.filePath(QStringLiteral("notes/escape.md"))));
        const QString folder = dir.filePath(QStringLiteral("notes"));
        const QString base = QUrl::fromLocalFile(folder + QLatin1Char('/')).toString();
        const auto linked = [&](const QString &relative) {
            return previewLinkedDocumentPath(QUrl(base).resolved(QUrl(relative)), base);
        };

        QCOMPARE(linked(QStringLiteral("a.md")), folder + QStringLiteral("/a.md"));
        QCOMPARE(linked(QStringLiteral("sub/b.md")), folder + QStringLiteral("/sub/b.md"));
        QCOMPARE(linked(QStringLiteral("./sub/../a.md")), folder + QStringLiteral("/a.md"));
        QCOMPARE(linked(QStringLiteral("upper.MD")), folder + QStringLiteral("/upper.MD"));
        // Not Markdown, not there, not in the folder, and a symlink out of it.
        QCOMPARE(linked(QStringLiteral("plain.txt")), QString());
        QCOMPARE(linked(QStringLiteral("missing.md")), QString());
        QCOMPARE(linked(QStringLiteral("../outside/other.md")), QString());
        QCOMPARE(linked(QStringLiteral("escape.md")), QString());
        QCOMPARE(linked(QStringLiteral("")), QString());
        QCOMPARE(previewLinkedDocumentPath(QUrl(QStringLiteral("https://example.com/a.md")), base),
                 QString());
        // With no folder, nothing is followable.
        QCOMPARE(previewLinkedDocumentPath(QUrl::fromLocalFile(folder + QStringLiteral("/a.md")),
                                           QString()), QString());
    }

    void resolvesDocumentLinks() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QDir dir(root.path());
        QVERIFY(dir.mkpath(QStringLiteral("sub")));
        for (const QString &name : {QStringLiteral("here.md"), QStringLiteral("there.md"),
                                    QStringLiteral("sub/deep.md"), QStringLiteral("notes.txt")}) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("x\n");
        }

        PreviewBridge bridge;
        QSignalSpy requested(&bridge, &PreviewBridge::documentLinkRequested);
        QSignalSpy refused(&bridge, &PreviewBridge::documentLinkRefused);

        // An unsaved document has no folder.
        bridge.openDocument(QStringLiteral("there.md"));
        QCOMPARE(requested.count(), 0);
        QCOMPARE(refused.count(), 1);
        QVERIFY(refused.takeFirst().constFirst().toString().contains(QStringLiteral("Save this")));

        bridge.setDocumentUrl(QUrl::fromLocalFile(dir.filePath(QStringLiteral("here.md"))));
        bridge.openDocument(QStringLiteral("there.md"));
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.at(0).at(0).toUrl(),
                 QUrl::fromLocalFile(dir.filePath(QStringLiteral("there.md"))));
        QCOMPARE(requested.at(0).at(1).toString(), QString());

        bridge.openDocument(QStringLiteral("sub/deep.md#results"));
        QCOMPARE(requested.count(), 2);
        QCOMPARE(requested.at(1).at(0).toUrl(),
                 QUrl::fromLocalFile(dir.filePath(QStringLiteral("sub/deep.md"))));
        QCOMPARE(requested.at(1).at(1).toString(), QStringLiteral("results"));

        // A neighbour that isn't there is named; everything else gets the rule.
        bridge.openDocument(QStringLiteral("gone.md"));
        QCOMPARE(requested.count(), 2);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.takeFirst().constFirst().toString(),
                 QStringLiteral("Could not find gone.md."));
        for (const QString &href : {QStringLiteral("notes.txt"), QStringLiteral("../elsewhere.md"),
                                    QStringLiteral("https://example.com/a.md"),
                                    QStringLiteral("mailto:writer@example.com")}) {
            bridge.openDocument(href);
            QCOMPARE(requested.count(), 2);
            QCOMPARE(refused.count(), 1);
            QVERIFY(refused.takeFirst().constFirst().toString().contains(QStringLiteral("folder")));
        }

        QSignalSpy navigation(&bridge, &PreviewBridge::historyNavigationRequested);
        bridge.navigateBack();
        bridge.navigateForward();
        QCOMPARE(navigation.count(), 2);
        QCOMPARE(navigation.at(0).constFirst().toInt(), -1);
        QCOMPARE(navigation.at(1).constFirst().toInt(), 1);
    }

    void keepsDocumentHistory() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QDir dir(root.path());
        const auto document = [&](const QString &name) {
            const QString path = dir.filePath(name);
            QFile file(path);
            file.open(QIODevice::WriteOnly);
            file.write("# note\n");
            file.close();
            return QUrl::fromLocalFile(path);
        };
        const QUrl a = document(QStringLiteral("a.md"));
        const QUrl b = document(QStringLiteral("b.md"));
        const QUrl c = document(QStringLiteral("c.md"));

        Backend backend;
        QSignalSpy restored(&backend, &Backend::previewHistoryRestored);
        // Opening a document records it; the first one has nowhere to go back to.
        backend.open(a);
        backend.previewHistoryVisited();
        QVERIFY(!backend.previewCanGoBack());
        QVERIFY(!backend.previewCanGoForward());
        QCOMPARE(backend.previewHistoryGo(-1, 0), QUrl());

        // Leaving a at caret 5 for b.
        backend.previewHistoryPrepare(5);
        backend.open(b);
        backend.previewHistoryVisited();
        QVERIFY(backend.previewCanGoBack());
        QVERIFY(!backend.previewCanGoForward());

        // Back to a, at the caret it was left at.
        const QUrl backTarget = backend.previewHistoryGo(-1, 12);
        QCOMPARE(backTarget, a);
        backend.open(backTarget);
        backend.previewHistoryVisited();
        QCOMPARE(restored.count(), 1);
        QCOMPARE(restored.takeFirst().constFirst().toInt(), 5);
        QVERIFY(backend.previewCanGoForward());
        QVERIFY(!backend.previewCanGoBack());

        // Forward to b, at its own caret.
        QCOMPARE(backend.previewHistoryGo(1, 0), b);
        backend.open(b);
        backend.previewHistoryVisited();
        QCOMPARE(restored.takeFirst().constFirst().toInt(), 12);
        QVERIFY(!backend.previewCanGoForward());

        // Going somewhere new after going back drops what was ahead.
        backend.open(backend.previewHistoryGo(-1, 0));
        backend.previewHistoryVisited();
        QVERIFY(backend.previewCanGoForward());
        backend.open(c);
        backend.previewHistoryVisited();
        QVERIFY(!backend.previewCanGoForward());
        QCOMPARE(backend.previewHistoryGo(-1, 0), a);

        // Reopening the same document doesn't add a step.
        Backend fresh;
        fresh.open(a);
        fresh.previewHistoryVisited();
        fresh.open(a);
        fresh.previewHistoryVisited();
        QVERIFY(!fresh.previewCanGoBack());

        // The history keeps the last 20 documents.
        Backend deep;
        for (int i = 0; i < 25; ++i) {
            deep.open(document(QStringLiteral("deep%1.md").arg(i)));
            deep.previewHistoryVisited();
        }
        int steps = 0;
        for (; steps < 30; ++steps) {
            const QUrl target = deep.previewHistoryGo(-1, 0);
            if (target.isEmpty())
                break;
            deep.open(target);
            deep.previewHistoryVisited();
        }
        // 20 kept, so 19 steps back from the newest.
        QCOMPARE(steps, 19);
    }

    // Alt+Left and Alt+Right walk the documents, in every build: this target
    // has no preview at all.
    void walksDocumentHistoryFromTheKeyboard() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto document = [&](const QString &name, const QByteArray &text) {
            const QString path = directory.filePath(name);
            QFile file(path);
            file.open(QIODevice::WriteOnly);
            file.write(text);
            file.close();
            return QUrl::fromLocalFile(path);
        };
        const QUrl first = document(QStringLiteral("first.md"), "# First\n\nSome prose here.\n");
        const QUrl second = document(QStringLiteral("second.md"), "# Second\n");

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> root(component.create());
        auto *window = qobject_cast<QQuickWindow *>(root.data());
        QVERIFY(window);
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));
        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);

        backend.open(first);
        QCOMPARE(backend.fileUrl(), first);
        QVERIFY(!backend.previewCanGoBack());
        // Nowhere to go yet.
        QTest::keyClick(window, Qt::Key_Left, Qt::AltModifier);
        QCOMPARE(backend.status(), QStringLiteral("Nowhere to go back to."));
        QCOMPARE(backend.fileUrl(), first);

        // Leaving first.md with the caret in the prose.
        editor->setProperty("cursorPosition", 12);
        backend.previewHistoryPrepare(12);
        backend.open(second);
        QVERIFY(backend.previewCanGoBack());

        QTest::keyClick(window, Qt::Key_Left, Qt::AltModifier);
        QCOMPARE(backend.fileUrl(), first);
        QCOMPARE(editor->property("cursorPosition").toInt(), 12);
        QVERIFY(backend.previewCanGoForward());

        QTest::keyClick(window, Qt::Key_Right, Qt::AltModifier);
        QCOMPARE(backend.fileUrl(), second);
        QVERIFY(!backend.previewCanGoForward());
        QTest::keyClick(window, Qt::Key_Right, Qt::AltModifier);
        QCOMPARE(backend.status(), QStringLiteral("Nowhere to go forward to."));
    }

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(OmaloremTest)
#include "tst_omalorem.moc"
