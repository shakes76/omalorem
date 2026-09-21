#include <QtTest>
#include <QFont>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>

#include "backend.h"
#include "markdownhighlighter.h"
#include "previewbridge.h"

class OmaviewTest : public QObject {
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
        QVERIFY(QMetaObject::invokeMethod(window.data(), "togglePreview"));
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
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nTextEdit {}", QUrl());
        QScopedPointer<QObject> editor(component.create());
        QVERIFY2(editor, qPrintable(component.errorString()));

        Backend backend;
        backend.attachDocument(editor->property("textDocument").value<QObject *>());
        PreviewBridge *bridge = backend.previewBridge();
        QVERIFY(bridge);
        QSignalSpy markdownSpy(bridge, &PreviewBridge::markdownChanged);

        editor->setProperty("text", QStringLiteral("$x^2$"));
        QVERIFY(backend.editorTextChanged());
        editor->setProperty("text", QStringLiteral("$x^3$"));
        QVERIFY(backend.editorTextChanged());
        QCOMPARE(markdownSpy.count(), 0);
        QTRY_COMPARE(markdownSpy.count(), 1);
        QCOMPARE(bridge->markdown(), QStringLiteral("$x^3$"));

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
        const QUrl image(QStringLiteral("file:///notes/figures/plot.png"));
        QVERIFY(bridge.isResourceAllowed(QUrl(QStringLiteral("qrc:/preview/index.html"))));
        QVERIFY(!bridge.isResourceAllowed(image));

        bridge.setDocumentUrl(QUrl::fromLocalFile(QStringLiteral("/notes/heat.md")));
        QVERIFY(bridge.isResourceAllowed(image));
        QVERIFY(bridge.isResourceAllowed(QUrl(QStringLiteral("file:///notes/a%20b.png"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("file:///notes/"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("file:///notes-other/x.png"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("file:///notes/../etc/passwd"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("file:///etc/passwd"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("file://host/notes/x.png"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("https://example.com/x.png"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("data:image/png;base64,AAAA"))));
        QVERIFY(!bridge.isResourceAllowed(QUrl(QStringLiteral("javascript:alert(1)"))));
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

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(OmaviewTest)
#include "tst_omaview.moc"
