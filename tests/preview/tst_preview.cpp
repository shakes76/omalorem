#include <QtTest>
#include <QApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickItem>
#include <QQuickWindow>

#include "backend.h"
#include "previewbridge.h"

namespace {

QString fixture(const QString &name) {
    QFile file(QStringLiteral(FIXTURES_DIR "/") + name);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

// Whether QtWebEngine has been loaded into this process yet.
bool webEngineLoaded() {
    QFile maps(QStringLiteral("/proc/self/maps"));
    return maps.open(QIODevice::ReadOnly) && maps.readAll().contains("libQt6WebEngineCore");
}

QUrl fixtureUrl(const QString &name) {
    return QUrl::fromLocalFile(QDir(QStringLiteral(FIXTURES_DIR)).absoluteFilePath(name));
}

} // namespace

class PreviewTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());

        // Nothing so far may have pulled in QtWebEngine: the test binary,
        // like omaview, does not link it.
        QVERIFY(!webEngineLoaded());
    }

    // The full editor with the preview available but hidden: Main.qml and
    // Backend, and not a byte of Chromium. Runs first, before anything in
    // this process has loaded the preview plugin.
    void keepsChromiumAwayWhileHidden() {
        QSettings().setValue(QStringLiteral("preview/visible"), false);
        auto editor = createEditor();
        QVERIFY(editor.window);
        QTest::qWait(300);
        QVERIFY(!previewWindow());
        QVERIFY(!webEngineLoaded());
        closeEditor(editor);
        QSettings().remove(QStringLiteral("preview"));
    }

    void loadsPreviewPage() {
        m_bridge = new PreviewBridge(this);
        m_bridge->setTheme(QStringLiteral("#0a0b0c"), QStringLiteral("#f0f1f2"),
                           QStringLiteral("#abcdef"), QStringLiteral("#123456"), true);
        m_engine = new QQmlEngine(this);
        m_engine->addImportPath(QCoreApplication::applicationDirPath());
        m_engine->rootContext()->setContextProperty(QStringLiteral("testBridge"), m_bridge);

        QQmlComponent component(m_engine, QUrl(QStringLiteral("qrc:/PreviewHarness.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        m_harness = component.create();
        QVERIFY2(m_harness, qPrintable(component.errorString()));
        QVERIFY(webEngineLoaded());

        m_sandbox = m_engine->singletonInstance<QObject *>(QStringLiteral("Omaview.Preview"),
                                                            QStringLiteral("PreviewSandbox"));
        QVERIFY(m_sandbox);
        QTRY_VERIFY(view());
        QObject *profile = view()->property("profile").value<QObject *>();
        QVERIFY(profile);
        QVERIFY(profile->property("offTheRecord").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(m_bridge->pageReady(), 20000);
    }

    void cleanupTestCase() {
        delete m_harness;
        m_harness = nullptr;
    }

    void rendersValidMath() {
        QVERIFY(renderFixture(QStringLiteral("math-valid.md")));
        // 3 inline ($a$, $b^2$, \(c + 1\)) plus 5 display blocks.
        QCOMPARE(js("document.querySelectorAll('#content .katex').length").toInt(), 8);
        QCOMPARE(js("document.querySelectorAll('#content .katex-display').length").toInt(), 5);
        QCOMPARE(js("document.querySelectorAll('#content .katex-error').length").toInt(), 0);
    }

    void rendersSampleWithoutErrors() {
        QVERIFY(renderFixture(QStringLiteral("sample.md")));
        QVERIFY(js("document.querySelectorAll('#content .katex-display').length").toInt() >= 5);
        QCOMPARE(js("document.querySelectorAll('#content .katex-error').length").toInt(), 0);
        // Task lists, tables and strikethrough come through.
        QCOMPARE(js("document.querySelectorAll('#content li.task-list-item input').length").toInt(), 2);
        QCOMPARE(js("document.querySelectorAll('#content table td').length").toInt(), 4);
        QCOMPARE(js("document.querySelectorAll('#content s').length").toInt(), 1);
    }

    void keepsPageFromScrollingSideways() {
        QVERIFY(renderFixture(QStringLiteral("sample.md")));
        const QVariantMap sizes = js(QStringLiteral(
            "(() => { const s = document.scrollingElement;"
            " const wide = [...document.querySelectorAll('.katex-display')]"
            "   .some(d => d.scrollWidth > d.clientWidth);"
            " return {scroll: s.scrollWidth, client: s.clientWidth, wide: wide}; })()")).toMap();
        QVERIFY(sizes.value(QStringLiteral("wide")).toBool());
        QCOMPARE(sizes.value(QStringLiteral("scroll")).toInt(),
                 sizes.value(QStringLiteral("client")).toInt());
    }

    void marksBadMathOnly() {
        QVERIFY(renderFixture(QStringLiteral("math-invalid.md")));
        QCOMPARE(js("document.querySelectorAll('#content .katex-error').length").toInt(), 2);
        QCOMPARE(js("document.querySelectorAll('#content .katex').length").toInt(), 1);
        QVERIFY(js("document.querySelector('#content .katex-error').title").toString()
                    .contains(QStringLiteral("ParseError")));
        // Shown in the accent colour, and the rest of the document renders.
        QCOMPARE(js("getComputedStyle(document.querySelector('.katex-error')).color").toString(),
                 QStringLiteral("rgb(171, 205, 239)"));
        QVERIFY(js("document.getElementById('content').textContent").toString()
                    .contains(QStringLiteral("The paragraph after the errors still renders.")));
    }

    void leavesCurrencyAlone() {
        QVERIFY(renderFixture(QStringLiteral("currency.md")));
        QCOMPARE(js("document.querySelectorAll('#content .katex, #content .katex-error').length").toInt(), 0);
        const QString text = js("document.getElementById('content').textContent").toString();
        QVERIFY(text.contains(QStringLiteral("cost $5 and the book $10.")));
        QVERIFY(text.contains(QStringLiteral("between $5 and $10, or $20")));
    }

    void keepsMathInCodeLiteral() {
        QVERIFY(renderFixture(QStringLiteral("code.md")));
        QCOMPARE(js("document.querySelectorAll('#content .katex').length").toInt(), 0);
        const QStringList code = js(QStringLiteral(
            "[...document.querySelectorAll('#content code')].map(c => c.textContent)"))
            .toStringList();
        QCOMPARE(code.size(), 3);
        QCOMPARE(code.at(0), QStringLiteral("$x^2$"));
        QVERIFY(code.at(1).contains(QStringLiteral("$$\ny = mx + c\n$$")));
        QVERIFY(code.at(2).contains(QStringLiteral("indented $z$ code")));
    }

    void escapesRawHtml() {
        QVERIFY(renderFixture(QStringLiteral("raw-html.md")));
        QCOMPARE(js("document.querySelectorAll('#content b, #content script, #content img').length").toInt(), 0);
        const QString text = js("document.getElementById('content').textContent").toString();
        QVERIFY(text.contains(QStringLiteral("<b>bold tag</b>")));
        QVERIFY(text.contains(QStringLiteral("<script>window.pwned = true;</script>")));
        QCOMPARE(js("window.pwned === undefined").toBool(), true);
    }

    void tagsBlocksWithSourceLines() {
        QVERIFY(renderFixture(QStringLiteral("math-valid.md")));
        const QStringList lines = js(QStringLiteral(
            "[...document.querySelectorAll('#content > [data-source-line]')]"
            ".map(e => e.dataset.sourceLine)")).toStringList();
        QCOMPARE(lines.mid(0, 4), QStringList({QStringLiteral("0"), QStringLiteral("2"),
                                               QStringLiteral("4"), QStringLiteral("8")}));
    }

    void followsThemeAndTextScale() {
        QVERIFY(renderFixture(QStringLiteral("sample.md")));
        m_bridge->setTheme(QStringLiteral("#fefefe"), QStringLiteral("#101010"),
                           QStringLiteral("#112233"), QStringLiteral("#445566"), false);
        m_bridge->setTextScale(1.5);
        QTRY_COMPARE(js("getComputedStyle(document.documentElement).backgroundColor").toString(),
                     QStringLiteral("rgb(254, 254, 254)"));
        QTRY_COMPARE(js("getComputedStyle(document.documentElement).fontSize").toString(),
                     QStringLiteral("30px"));
        QCOMPARE(js("getComputedStyle(document.querySelector('#content a')).color").toString(),
                 QStringLiteral("rgb(17, 34, 51)"));
        QCOMPARE(js("getComputedStyle(document.documentElement).getPropertyValue('--muted').trim()")
                     .toString(), QStringLiteral("#aeb1b5"));
        QVERIFY(js("getComputedStyle(document.body).fontFamily").toString()
                    .contains(QStringLiteral("iA Writer Mono S")));
        QTRY_VERIFY(js("document.fonts.check('20px \"iA Writer Mono S\"')").toBool());

        m_bridge->setTheme(QStringLiteral("#0a0b0c"), QStringLiteral("#f0f1f2"),
                           QStringLiteral("#abcdef"), QStringLiteral("#123456"), true);
        m_bridge->setTextScale(1.0);
        QTRY_COMPARE(js("getComputedStyle(document.documentElement).fontSize").toString(),
                     QStringLiteral("20px"));
    }

    void keepsScrollPositionAcrossRenders() {
        QString longDocument = fixture(QStringLiteral("sample.md"));
        for (int i = 0; i < 4; ++i)
            longDocument += longDocument;
        QVERIFY(renderMarkdown(longDocument, QStringLiteral("scroll-1")));
        js("document.scrollingElement.scrollTop = 1500");
        QCOMPARE(js("document.scrollingElement.scrollTop").toInt(), 1500);
        QVERIFY(renderMarkdown(longDocument + QStringLiteral("\n\nmore"), QStringLiteral("scroll-2")));
        QCOMPARE(js("document.scrollingElement.scrollTop").toInt(), 1500);
    }

    void makesNoOutsideRequests() {
        // Everything rendered so far loaded only from qrc: and the document.
        QCOMPARE(blockedRequests(), QStringList());
    }

    void blocksRemoteAndOutsideResources() {
        m_bridge->setDocumentUrl(fixtureUrl(QStringLiteral("remote.md")));
        QVERIFY(renderFixture(QStringLiteral("remote.md")));
        // The remote image and the one outside the folder never load: the
        // page's CSP and Chromium's own file: checks stop them before the
        // interceptor even sees them.
        QTRY_COMPARE(js("[...document.images].filter(i => i.complete).length").toInt(), 3);
        const QString loaded = js("[...document.images].filter(i => i.naturalWidth > 0)"
                                  ".map(i => i.alt).join(',')").toString();
        m_bridge->setDocumentUrl(QUrl());
        QVERIFY(!loaded.contains(QStringLiteral("remote")));
        QVERIFY(!loaded.contains(QStringLiteral("outside")));
        // Chromium refuses every file: subresource to a qrc: page, including
        // the image beside the document. Local images are M2 (spec 4.4) and
        // need a different route, such as a scheme handler for the folder.
        QEXPECT_FAIL("", "file: images are refused to the qrc: page; local images are M2", Continue);
        QCOMPARE(loaded, QStringLiteral("local"));
    }

    void interceptorBlocksWhatGetsPastThePage() {
        // A bare view on the sandboxed profile, with no page and no CSP in
        // front, loading URLs the interceptor has to refuse by itself.
        m_bridge->setDocumentUrl(fixtureUrl(QStringLiteral("remote.md")));
        QQmlContext context(m_engine->rootContext());
        context.setContextProperty(QStringLiteral("testProfile"), view()->property("profile"));
        QQmlComponent component(m_engine);
        component.setData("import QtQuick\nimport QtWebEngine\n"
                          "WebEngineView { width: 200; height: 200; profile: testProfile }",
                          QUrl(QStringLiteral("qrc:/BareView.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> bareView(component.create(&context));
        QVERIFY(bareView);

        const QUrl local = fixtureUrl(QStringLiteral("pixel.png"));
        const QStringList targets{QStringLiteral("https://example.com/"),
                                  QStringLiteral("file:///etc/hosts"),
                                  local.toString()};
        for (const QString &target : targets) {
            QSignalSpy loadSpy(bareView.data(), SIGNAL(loadingChanged(QWebEngineLoadingInfo)));
            bareView->setProperty("url", QUrl(target));
            QTRY_VERIFY_WITH_TIMEOUT(!bareView->property("loading").toBool() && loadSpy.count() >= 2, 10000);
        }
        QCOMPARE(blockedRequests(),
                 QStringList({QStringLiteral("https://example.com/"),
                              QStringLiteral("file:///etc/hosts")}));
        m_bridge->setDocumentUrl(QUrl());
    }

    void routesLinksThroughTheBridge() {
        QVERIFY(renderFixture(QStringLiteral("remote.md")));
        QSignalSpy linkSpy(m_bridge, &PreviewBridge::externalLinkRequested);
        js("document.querySelector('#content a').click(); true");
        QTRY_COMPARE(linkSpy.count(), 1);
        QCOMPARE(linkSpy.at(0).constFirst().toUrl(),
                 QUrl(QStringLiteral("https://example.com/page")));

        // Script navigation and pop-ups are refused, the page stays put.
        js("location.href = 'https://example.com/elsewhere'; true");
        js("window.open('https://example.com/popup'); true");
        QTest::qWait(300);
        QCOMPARE(js("location.href").toString(), QStringLiteral("qrc:/preview/index.html"));
        QCOMPARE(js("document.getElementById('content') !== null").toBool(), true);
    }

    // The full editor with the preview available: Main.qml, Backend and the
    // real PreviewWindow from the plugin.
    void opensPreviewWindowBesideEditor() {
        auto editor = createEditor();
        QVERIFY(editor.window);
        QTRY_VERIFY(previewWindow());
        QQuickWindow *preview = previewWindow();
        QTRY_VERIFY(preview->isVisible());
        QCOMPARE(preview->transientParent(), nullptr);
        QCOMPARE(preview->title(), QStringLiteral("Preview — Untitled.md - Omaview"));

        editor.backend->open(fixtureUrl(QStringLiteral("math-valid.md")));
        QCOMPARE(preview->title(), QStringLiteral("Preview — math-valid.md - Omaview"));
        QTRY_VERIFY_WITH_TIMEOUT(editor.backend->previewBridge()->pageReady(), 20000);

        // Ctrl+E works with focus in the preview window, and hides it.
        preview->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(preview));
        QTest::keyClick(preview, Qt::Key_E, Qt::ControlModifier);
        QCOMPARE(editor.backend->previewVisible(), false);
        QTRY_VERIFY(!preview->isVisible());
        QCOMPARE(QSettings().value(QStringLiteral("preview/visible")).toBool(), false);

        // The same shortcut from the editor shows it again, same window.
        editor.window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(editor.window));
        QTest::keyClick(editor.window, Qt::Key_E, Qt::ControlModifier);
        QTRY_VERIFY(preview->isVisible());
        QCOMPARE(previewWindow(), preview);

        // Closing the preview only hides it.
        preview->close();
        QTRY_VERIFY(!preview->isVisible());
        QCOMPARE(editor.backend->previewVisible(), false);
        QVERIFY(editor.window->isVisible());

        // Closing the editor closes the preview but leaves the setting alone.
        editor.backend->setPreviewVisible(true);
        QTRY_VERIFY(preview->isVisible());
        QPointer<QQuickWindow> previewGuard(preview);
        editor.window->close();
        QTRY_VERIFY(!previewGuard || !previewGuard->isVisible());
        QCOMPARE(editor.backend->previewVisible(), true);
        closeEditor(editor);
        QSettings().remove(QStringLiteral("preview"));
    }

    void handsFindToEditorAndKeepsF11InPreview() {
        auto editor = createEditor();
        QVERIFY(editor.window);
        QTRY_VERIFY(previewWindow());
        QQuickWindow *preview = previewWindow();
        QTRY_VERIFY(preview->isVisible());
        QTRY_VERIFY_WITH_TIMEOUT(editor.backend->previewBridge()->pageReady(), 20000);

        // Ctrl+F from the preview opens find in the editor and moves the
        // keyboard there.
        preview->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(preview));
        QTest::keyClick(preview, Qt::Key_F, Qt::ControlModifier);
        QCOMPARE(editor.window->property("searchOpen").toBool(), true);
        QTRY_VERIFY(editor.window->isActive());

        // F11 in the preview fullscreens the preview, not the editor...
        preview->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(preview));
        QTest::keyClick(preview, Qt::Key_F11);
        QTRY_COMPARE(preview->visibility(), QWindow::FullScreen);
        QVERIFY(editor.window->visibility() != QWindow::FullScreen);

        // ...also with the web view itself focused.
        QTest::mouseClick(preview, Qt::LeftButton, {}, QPoint(preview->width() / 2, 40));
        QTRY_VERIFY(preview->activeFocusItem()
                    && QByteArray(preview->activeFocusItem()->metaObject()->className())
                           .contains("WebEngine"));
        QTest::keyClick(preview, Qt::Key_F11);
        QTRY_VERIFY(preview->visibility() != QWindow::FullScreen);
        QVERIFY(editor.window->visibility() != QWindow::FullScreen);

        // In the editor, F11 still does what it always did.
        editor.window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(editor.window));
        QTest::keyClick(editor.window, Qt::Key_F11);
        QTRY_COMPARE(editor.window->visibility(), QWindow::FullScreen);
        QVERIFY(preview->visibility() != QWindow::FullScreen);

        closeEditor(editor);
        QSettings().remove(QStringLiteral("preview"));
    }

private:
    struct Editor {
        Backend *backend = nullptr;
        QQmlEngine *engine = nullptr;
        QQuickWindow *window = nullptr;
    };

    Editor createEditor() {
        Editor editor;
        editor.backend = new Backend;
        editor.backend->setPreviewAvailable(true);
        editor.engine = new QQmlEngine;
        editor.engine->addImportPath(QCoreApplication::applicationDirPath());
        editor.engine->rootContext()->setContextProperty(QStringLiteral("backend"), editor.backend);
        QQmlComponent component(editor.engine, QUrl(QStringLiteral("qrc:/Main.qml")));
        if (!component.isReady()) {
            qWarning() << component.errorString();
            return editor;
        }
        editor.window = qobject_cast<QQuickWindow *>(component.create());
        return editor;
    }

    void closeEditor(Editor &editor) {
        delete editor.window;
        delete editor.engine;
        delete editor.backend;
        editor = {};
    }

    static QQuickWindow *previewWindow() {
        const QWindowList windows = QGuiApplication::topLevelWindows();
        for (QWindow *window : windows) {
            if (window->title().startsWith(QStringLiteral("Preview — ")))
                return qobject_cast<QQuickWindow *>(window);
        }
        return nullptr;
    }

    QObject *view() const {
        QObject *pane = m_harness ? m_harness->property("pane").value<QObject *>() : nullptr;
        return pane ? pane->property("view").value<QObject *>() : nullptr;
    }

    QStringList blockedRequests() const {
        QStringList blocked;
        QMetaObject::invokeMethod(m_sandbox, "blockedRequests", Q_RETURN_ARG(QStringList, blocked));
        return blocked;
    }

    QVariant js(const QString &script) {
        const int serial = m_harness->property("resultSerial").toInt();
        QMetaObject::invokeMethod(m_harness, "evaluate", Q_ARG(QVariant, script));
        if (!QTest::qWaitFor([&]() {
                return m_harness->property("resultSerial").toInt() > serial;
            }, 10000))
            return {};
        return m_harness->property("lastResult");
    }
    QVariant js(const char *script) { return js(QString::fromUtf8(script)); }

    // Renders markdown and waits until the page shows it: a marker paragraph
    // at the end tells this render apart from the previous one.
    bool renderMarkdown(const QString &markdown, const QString &marker) {
        m_bridge->setMarkdown(markdown + QStringLiteral("\n\nend-of-") + marker + QLatin1Char('\n'));
        const QString probe = QStringLiteral(
            "(() => { const p = document.querySelector('#content > p:last-of-type');"
            " return p ? p.textContent : ''; })()");
        return QTest::qWaitFor([&]() {
            return js(probe).toString() == QStringLiteral("end-of-") + marker;
        }, 10000);
    }

    bool renderFixture(const QString &name) {
        const QString markdown = fixture(name);
        if (markdown.isEmpty())
            return false;
        return renderMarkdown(markdown, name + QString::number(++m_renderCount));
    }

    QTemporaryDir m_settingsDirectory;
    PreviewBridge *m_bridge = nullptr;
    QObject *m_sandbox = nullptr;
    QQmlEngine *m_engine = nullptr;
    QObject *m_harness = nullptr;
    int m_renderCount = 0;
};

int main(int argc, char *argv[]) {
    // As in the application: WebEngine arrives with the preview plugin and
    // needs context sharing set before the QApplication exists.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    PreviewTest test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_preview.moc"
