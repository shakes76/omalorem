#include <QtTest>
#include <QApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPdfDocument>
#include <QBuffer>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QThread>
#include <QPdfSelection>

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

// Stands in for xdg-desktop-portal's Print interface, so the portal path
// runs without a real dialog. It lives on its own thread and connection:
// the client's blocking availability check would otherwise wait on itself.
class MockPrintPortal : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Print")
    Q_PROPERTY(uint version READ version)

public:
    static constexpr const char *service = "org.omalorem.TestPrintPortal";

    explicit MockPrintPortal(const QDBusConnection &bus) : m_bus(bus) {}
    uint version() const { return 4; }

    // Set before a print; read after it has finished.
    uint prepareResponse = 0;
    QVariantMap pageSetup;
    std::atomic<int> printCalls{0};
    uint printedToken = 0;
    QByteArray printed;

public slots:
    QDBusObjectPath PreparePrint(const QString &, const QString &, const QVariantMap &,
                                 const QVariantMap &, const QVariantMap &options,
                                 const QDBusMessage &message) {
        const QString path = requestPath(message, options);
        respond(path, prepareResponse,
                {{QStringLiteral("settings"), QVariantMap()},
                 {QStringLiteral("page-setup"), pageSetup},
                 {QStringLiteral("token"), 42u}});
        return QDBusObjectPath(path);
    }

    QDBusObjectPath Print(const QString &, const QString &, const QDBusUnixFileDescriptor &fd,
                          const QVariantMap &options, const QDBusMessage &message) {
        printedToken = options.value(QStringLiteral("token")).toUInt();
        QFile file;
        if (file.open(fd.fileDescriptor(), QIODevice::ReadOnly))
            printed = file.readAll();
        ++printCalls;
        const QString path = requestPath(message, options);
        respond(path, 0, {});
        return QDBusObjectPath(path);
    }

private:
    static QString requestPath(const QDBusMessage &message, const QVariantMap &options) {
        QString sender = message.service().mid(1);
        sender.replace(QLatin1Char('.'), QLatin1Char('_'));
        return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2")
            .arg(sender, options.value(QStringLiteral("handle_token")).toString());
    }

    // After the reply, as the real portal does once its dialog closes.
    void respond(const QString &path, uint response, const QVariantMap &results) {
        QTimer::singleShot(20, this, [this, path, response, results] {
            QDBusMessage signal = QDBusMessage::createSignal(
                path, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"));
            signal << response << results;
            m_bus.send(signal);
        });
    }

    QDBusConnection m_bus;
};

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
        // like omalorem, does not link it.
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

        m_sandbox = m_engine->singletonInstance<QObject *>(QStringLiteral("Omalorem.Preview"),
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

    void followsFontFamily() {
        QVERIFY(renderFixture(QStringLiteral("sample.md")));
        const QString proseFont = QStringLiteral(
            "getComputedStyle(document.querySelector('#content > p')).fontFamily");
        const QString codeFont = QStringLiteral(
            "getComputedStyle(document.querySelector('#content code')).fontFamily");
        const QString columnWidth = QStringLiteral(
            "document.getElementById('content').getBoundingClientRect().width");
        QVERIFY(js(proseFont).toString().startsWith(QStringLiteral("\"iA Writer Mono S\"")));
        const double monoWidth = js(columnWidth).toDouble();

        m_bridge->setFontFamily(QStringLiteral("quattro"));
        QTRY_VERIFY(js(proseFont).toString().startsWith(QStringLiteral("\"iA Writer Quattro S\"")));
        // The bundled face really loads, code stays in Mono, and the column
        // keeps the editor's measure.
        QTRY_VERIFY(js("document.fonts.check('20px \"iA Writer Quattro S\"')").toBool());
        QTRY_VERIFY(js("[...document.fonts].some(f => f.family.includes('Quattro')"
                       " && f.status === 'loaded')").toBool());
        QVERIFY(js(codeFont).toString().startsWith(QStringLiteral("\"iA Writer Mono S\"")));
        QCOMPARE(js(columnWidth).toDouble(), monoWidth);

        m_bridge->setFontFamily(QStringLiteral("mono"));
        QTRY_VERIFY(js(proseFont).toString().startsWith(QStringLiteral("\"iA Writer Mono S\"")));
    }

    void keepsScrollPositionAcrossRenders() {
        QString longDocument = fixture(QStringLiteral("sample.md"));
        for (int i = 0; i < 4; ++i)
            longDocument += longDocument;
        QVERIFY(renderMarkdown(longDocument, QStringLiteral("scroll-1")));
        // The reader scrolls the preview itself, so it stops following the
        // editor and stays where they put it.
        js("document.dispatchEvent(new WheelEvent('wheel'));"
           " document.scrollingElement.scrollTop = 1500");
        QCOMPARE(js("document.scrollingElement.scrollTop").toInt(), 1500);
        QVERIFY(renderMarkdown(longDocument + QStringLiteral("\n\nmore"), QStringLiteral("scroll-2")));
        QCOMPARE(js("document.scrollingElement.scrollTop").toInt(), 1500);
    }

    // Editor → preview: the block at the bridge's source line sits at the
    // top of the view, below the page's margin, as the line does in the
    // editor; fractions interpolate within a block and across the blank
    // lines between blocks.
    void followsEditorSourceLine() {
        QStringList lines = longDocument().split(QLatin1Char('\n'));
        QVERIFY(renderMarkdown(lines.join(QLatin1Char('\n')), QStringLiteral("sync-1")));
        const QString marginTop = QStringLiteral(
            "parseFloat(getComputedStyle(document.getElementById('content')).paddingTop)");
        const int margin = js(marginTop).toInt();
        QVERIFY(margin >= 42);
        // Section 30's heading is line 600, its display math lines 604-606.
        const QString headingTop = QStringLiteral(
            "document.getElementById('section-30').getBoundingClientRect().top");

        m_bridge->setSourceLine(600);
        QTRY_COMPARE(qRound(js(headingTop).toDouble()), margin);

        const QString mathBlock = QStringLiteral(
            "document.querySelector('#content > [data-source-line=\"604\"]')");
        QCOMPARE(js(mathBlock + QStringLiteral(".dataset.sourceLineEnd")).toString(),
                 QStringLiteral("607"));
        m_bridge->setSourceLine(605.5);
        QTRY_COMPARE(qRound(js(QStringLiteral("(() => { const r = %1.getBoundingClientRect();"
                                              " return r.top + r.height / 2; })()").arg(mathBlock))
                                .toDouble()),
                     margin);
        // Line 603 is the blank line between the paragraph and the math.
        m_bridge->setSourceLine(603.5);
        QTRY_VERIFY(js(QStringLiteral("(() => { const m = %1.getBoundingClientRect().top;"
                                      " const p = %1.previousElementSibling.getBoundingClientRect().bottom;"
                                      " return %2 > p && %2 < m; })()").arg(mathBlock).arg(margin))
                        .toBool());

        // A render that makes a block above taller keeps the heading lined up.
        m_bridge->setSourceLine(600);
        QTRY_COMPARE(qRound(js(headingTop).toDouble()), margin);
        lines[8] += QStringLiteral(" And much more prose, enough to wrap onto more lines than"
                                   " before in the preview's column, and then some more.");
        QVERIFY(renderMarkdown(lines.join(QLatin1Char('\n')), QStringLiteral("sync-2")));
        QCOMPARE(qRound(js(headingTop).toDouble()), margin);

        // Once the reader scrolls the preview, renders leave it alone, until
        // the editor scrolls again.
        js("document.dispatchEvent(new WheelEvent('wheel'));"
           " document.scrollingElement.scrollTop -= 300; true");
        const int readerTop = js("document.scrollingElement.scrollTop").toInt();
        lines[8] += QStringLiteral(" Still more.");
        QVERIFY(renderMarkdown(lines.join(QLatin1Char('\n')), QStringLiteral("sync-3")));
        QCOMPARE(js("document.scrollingElement.scrollTop").toInt(), readerTop);
        m_bridge->setSourceLine(0);
        QTRY_COMPARE(js("document.scrollingElement.scrollTop").toInt(), 0);
    }

    // Spec 5.4: typing in one paragraph re-renders only that paragraph, and
    // an update of the 2,000-line, 200-equation document fits in 16 ms. The
    // timing is logged, not gated; the node reuse is.
    void patchesOnlyChangedBlocks() {
        const QString document = longDocument();
        QCOMPARE(document.count(QLatin1Char('\n')), 2000);
        QVERIFY(renderMarkdown(document, QStringLiteral("patch-0")));
        const QVariantMap first = lastRender();
        const int blocks = first.value(QStringLiteral("blocks")).toInt();
        QCOMPARE(js("document.querySelectorAll('#content .katex').length").toInt(), 200);
        QCOMPARE(js("document.querySelectorAll('#content .katex-error').length").toInt(), 0);

        // Mark the typeset nodes; they must survive edits elsewhere.
        js("document.querySelectorAll('#content .katex').forEach(k => k.omaloremMark = true); true");
        QList<double> timings;
        QStringList lines = document.split(QLatin1Char('\n'));
        for (int edit = 1; edit <= 15; ++edit) {
            // Type into a paragraph in the middle of the document.
            lines[1008] += QStringLiteral(" word");
            QVERIFY(renderMarkdown(lines.join(QLatin1Char('\n')),
                                   QStringLiteral("patch-") + QString::number(edit)));
            const QVariantMap stats = lastRender();
            QCOMPARE(stats.value(QStringLiteral("blocks")).toInt(), blocks);
            // The edited paragraph and the end marker.
            QCOMPARE(stats.value(QStringLiteral("created")).toInt(), 2);
            timings.append(stats.value(QStringLiteral("ms")).toDouble());
        }
        QCOMPARE(js("[...document.querySelectorAll('#content .katex')]"
                    ".filter(k => k.omaloremMark).length").toInt(), 200);

        // A line added at the top shifts every block's source line without
        // re-creating any of them.
        lines.prepend(QString());
        QVERIFY(renderMarkdown(lines.join(QLatin1Char('\n')), QStringLiteral("patch-shift")));
        QCOMPARE(lastRender().value(QStringLiteral("created")).toInt(), 1);
        QCOMPARE(js("document.querySelector('#content > h2').dataset.sourceLine").toString(),
                 QStringLiteral("1"));
        QCOMPARE(js("[...document.querySelectorAll('#content .katex')]"
                    ".filter(k => k.omaloremMark).length").toInt(), 200);

        // The first update after loading also pays for one full relayout
        // once KaTeX's web fonts arrive, so it is reported on its own.
        const double firstUpdate = timings.takeFirst();
        std::sort(timings.begin(), timings.end());
        qInfo("Render of the 2,000-line document: load %.1f ms, first update %.1f ms,"
              " then median %.1f ms and worst %.1f ms (budget 16 ms, spec 5.4)",
              first.value(QStringLiteral("ms")).toDouble(), firstUpdate,
              timings.at(timings.size() / 2), timings.last());
    }

    void makesNoOutsideRequests() {
        // Everything rendered so far loaded only from qrc: and the document.
        QCOMPARE(blockedRequests(), QStringList());
    }

    void showsOnlyDocumentFolderImages() {
        m_bridge->setDocumentUrl(fixtureUrl(QStringLiteral("remote.md")));
        QVERIFY(renderFixture(QStringLiteral("remote.md")));
        // The image beside the document loads through omalorem-doc:; the
        // remote one and the one outside the folder are placeholders and
        // are never requested at all.
        QTRY_VERIFY(js("[...document.images].every(i => i.complete)").toBool());
        QCOMPARE(js("[...document.images].filter(i => i.naturalWidth > 0).map(i => i.alt)"
                    ".join(',')").toString(), QStringLiteral("local"));
        QCOMPARE(js("document.images[0].getAttribute('src')").toString(),
                 QStringLiteral("omalorem-doc:/pixel.png"));
        QCOMPARE(js("[...document.querySelectorAll('#content .image-placeholder')]"
                    ".map(p => p.textContent + ':' + p.title).join('|')").toString(),
                 QStringLiteral("remote:Remote images are not shown|"
                                "outside:Only images in the document's folder are shown"));

        // An unsaved document has no folder, so even that image is a
        // placeholder; changing the folder re-renders.
        m_bridge->setDocumentUrl(QUrl());
        QTRY_COMPARE(js("document.images.length").toInt(), 0);
        QCOMPARE(js("document.querySelectorAll('#content .image-placeholder').length").toInt(), 3);
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

        // Not even the document's own folder is reachable through file:.
        const QUrl local = fixtureUrl(QStringLiteral("pixel.png"));
        const QStringList targets{QStringLiteral("https://example.com/"),
                                  QStringLiteral("file:///etc/hosts"),
                                  local.toString(),
                                  QStringLiteral("omalorem-doc:/pixel.png")};
        for (const QString &target : targets) {
            QSignalSpy loadSpy(bareView.data(), SIGNAL(loadingChanged(QWebEngineLoadingInfo)));
            bareView->setProperty("url", QUrl(target));
            QTRY_VERIFY_WITH_TIMEOUT(!bareView->property("loading").toBool() && loadSpy.count() >= 2, 10000);
        }
        QCOMPARE(blockedRequests(),
                 QStringList({QStringLiteral("https://example.com/"),
                              QStringLiteral("file:///etc/hosts"),
                              local.toString()}));
        m_bridge->setDocumentUrl(QUrl());
    }

    // The scheme handler serves image files from the document's folder and
    // its subfolders, and nothing else: no symlinks out, no other files.
    void servesImagesFromDocumentFolder() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QDir dir(root.path());
        QVERIFY(dir.mkpath(QStringLiteral("doc/sub")) && dir.mkpath(QStringLiteral("outside")));
        const QString pixel = QStringLiteral(FIXTURES_DIR "/pixel.png");
        for (const QString &name : {QStringLiteral("doc/a.png"), QStringLiteral("doc/sub/b.png"),
                                    QStringLiteral("doc/my pic.png"),
                                    QStringLiteral("outside/c.png")})
            QVERIFY(QFile::copy(pixel, dir.filePath(name)));
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("outside/c.png")),
                            dir.filePath(QStringLiteral("doc/escape.png"))));
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("doc/a.png")),
                            dir.filePath(QStringLiteral("doc/alias.png"))));
        QFile text(dir.filePath(QStringLiteral("doc/notes.txt")));
        QVERIFY(text.open(QIODevice::WriteOnly));
        text.write("private notes");
        text.close();

        m_bridge->setDocumentUrl(QUrl::fromLocalFile(dir.filePath(QStringLiteral("doc/doc.md"))));
        QVERIFY(renderMarkdown(QStringLiteral(
            "![a](a.png) ![b](sub/b.png) ![spaced](my%20pic.png) ![alias](./alias.png)\n"
            "![file](%1) ![escape](escape.png) ![text](notes.txt) ![missing](nope.png)\n")
            .arg(QUrl::fromLocalFile(dir.filePath(QStringLiteral("doc/a.png"))).toString()),
            QStringLiteral("folder")));
        QTRY_VERIFY(js("[...document.images].every(i => i.complete)").toBool());
        QCOMPARE(js("[...document.images].filter(i => i.naturalWidth > 0).map(i => i.alt)"
                    ".join(',')").toString(), QStringLiteral("a,b,spaced,alias,file"));
        QCOMPARE(js("[...document.images].filter(i => i.naturalWidth === 0).map(i => i.alt)"
                    ".join(',')").toString(), QStringLiteral("escape,text,missing"));
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

    void scrollsToHeadingAnchors() {
        QString markdown = QStringLiteral("# Top\n\n[down](#second-part-x) [again](#notes-1)\n\n");
        for (int i = 0; i < 60; ++i)
            markdown += QStringLiteral("Filler paragraph %1.\n\n").arg(i);
        markdown += QStringLiteral("## Second *part*: $x$!\n\n## Notes\n\n## Notes\n\n");
        // Enough below the heading for it to reach the top of the view.
        for (int i = 0; i < 60; ++i)
            markdown += QStringLiteral("More filler %1.\n\n").arg(i);
        QVERIFY(renderMarkdown(markdown, QStringLiteral("anchors")));
        QCOMPARE(js("[...document.querySelectorAll('#content h1, #content h2')].map(h => h.id)"
                    ".join(',')").toString(),
                 QStringLiteral("top,second-part-x,notes,notes-1"));

        QSignalSpy linkSpy(m_bridge, &PreviewBridge::externalLinkRequested);
        js("document.scrollingElement.scrollTop = 0; document.querySelector('a[href=\"#second-part-x\"]').click(); true");
        QTRY_VERIFY(js("document.scrollingElement.scrollTop").toInt() > 0);
        QCOMPARE(js("Math.round(document.getElementById('second-part-x').getBoundingClientRect().top)")
                     .toInt(), 0);
        QCOMPARE(js("location.href").toString(), QStringLiteral("qrc:/preview/index.html"));
        QCOMPARE(linkSpy.count(), 0);
        js("document.scrollingElement.scrollTop = 0; true");
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
        QCOMPARE(preview->title(), QStringLiteral("Preview — Untitled.md - Omalorem"));

        editor.backend->open(fixtureUrl(QStringLiteral("math-valid.md")));
        QCOMPARE(preview->title(), QStringLiteral("Preview — math-valid.md - Omalorem"));
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

    // A black view after a workspace switch: the page can be asked for a
    // fresh frame, and the nudge leaves nothing behind.
    void repaintsOnRequest() {
        QVERIFY(renderFixture(QStringLiteral("sample.md")));
        const int before = js("window.omaloremPreview.repaints").toInt();
        QObject *pane = m_harness->property("pane").value<QObject *>();
        QVERIFY(QMetaObject::invokeMethod(pane, "repaint"));
        QTRY_COMPARE(js("window.omaloremPreview.repaints").toInt(), before + 1);
        QTRY_COMPARE(js("document.documentElement.style.opacity").toString(), QString());
    }

    void watchesForReexposure() {
        QQmlComponent component(m_engine);
        component.setData("import QtQuick\nimport Omalorem.Preview\n"
                          "Window { id: window; width: 100; height: 100; visible: true\n"
                          "  property alias watcher: watcher\n"
                          "  PreviewExposeWatcher { id: watcher; window: window } }",
                          QUrl(QStringLiteral("qrc:/ExposeHarness.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.create());
        auto *window = qobject_cast<QQuickWindow *>(object.data());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QObject *watcher = window->property("watcher").value<QObject *>();
        QVERIFY(watcher);
        QCOMPARE(watcher->property("window").value<QWindow *>(), window);

        QSignalSpy reexposed(watcher, SIGNAL(reexposed()));
        window->hide();
        QTRY_VERIFY(!window->isExposed());
        QCOMPARE(reexposed.count(), 0);
        window->show();
        QTRY_COMPARE(reexposed.count(), 1);
    }

    void repaintsPreviewWhenItComesBack() {
        auto editor = createEditor();
        QVERIFY(editor.window);
        QTRY_VERIFY(previewWindow());
        QQuickWindow *preview = previewWindow();
        QTRY_VERIFY(preview->isExposed());
        QObject *pane = preview->findChild<QObject *>(QStringLiteral("previewPane"));
        QVERIFY(pane);
        QTRY_VERIFY_WITH_TIMEOUT(editor.backend->previewBridge()->pageReady(), 20000);

        // Hidden and shown again, as the window system does on a workspace
        // switch: the preview asks for a fresh frame, then once more.
        int requests = pane->property("repaintRequests").toInt();
        preview->hide();
        QTRY_VERIFY(!preview->isExposed());
        preview->show();
        QTRY_VERIFY(pane->property("repaintRequests").toInt() >= requests + 2);

        // Focus coming back to the editor asks as well.
        preview->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(preview));
        requests = pane->property("repaintRequests").toInt();
        editor.window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(editor.window));
        QTRY_VERIFY(pane->property("repaintRequests").toInt() > requests);

        closeEditor(editor);
        QSettings().remove(QStringLiteral("preview"));
    }

    // Main.qml publishes the line at the top of the editor's view, with the
    // fraction scrolled past, as the editor scrolls.
    void publishesEditorSourceLine() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("long.md"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QString document = longDocument();
        file.write(document.toUtf8());
        file.close();

        auto editor = createEditor();
        QVERIFY(editor.window);
        editor.backend->open(QUrl::fromLocalFile(path));
        QTRY_VERIFY(previewWindow());
        auto *text = editor.window->findChild<QQuickItem *>(QStringLiteral("sourceEditor"));
        QVERIFY(text);
        QQuickItem *flick = text->parentItem()->parentItem();
        QVERIFY(flick && flick->property("contentY").isValid());
        PreviewBridge *bridge = editor.backend->previewBridge();

        // Scroll the editor so line 600 starts exactly at the probe.
        int position = 0;
        for (int line = 0; line < 600; ++line)
            position = document.indexOf(QLatin1Char('\n'), position) + 1;
        QRectF lineRect;
        QVERIFY(QMetaObject::invokeMethod(text, "positionToRectangle", Q_RETURN_ARG(QRectF, lineRect),
                                          Q_ARG(int, position)));
        flick->setProperty("contentY", lineRect.y());
        QTRY_COMPARE(bridge->sourceLine(), 600.0);

        // Halfway down that line's height.
        flick->setProperty("contentY", lineRect.y() + lineRect.height() / 2);
        QTRY_VERIFY(qAbs(bridge->sourceLine() - 600.5) < 0.01);

        // Hidden, the preview gets no updates.
        editor.backend->setPreviewVisible(false);
        flick->setProperty("contentY", 0);
        QTest::qWait(50);
        QVERIFY(bridge->sourceLine() > 600);
        // Shown again, it catches up at once.
        editor.backend->setPreviewVisible(true);
        QTRY_COMPARE(bridge->sourceLine(), 0.0);

        closeEditor(editor);
        QSettings().remove(QStringLiteral("preview"));
    }

    // Wide display math is scaled to fit the printed column.
    void scalesWideMathForPrint() {
        QVERIFY(renderFixture(QStringLiteral("sample.md")));
        QVERIFY(js("window.omaloremPreview.preparePrint(210)").toInt() >= 1);
        const double zoom = js("Math.min(...[...document.querySelectorAll('.katex-display')]"
                               ".map(d => parseFloat(d.style.getPropertyValue('--print-zoom')) || 1))")
                                .toDouble();
        QVERIFY(zoom > 0.1 && zoom < 1);
        // On screen nothing changes: the zoom is used by print only.
        QCOMPARE(js("getComputedStyle(document.querySelector('.katex-display')).zoom").toString(),
                 QStringLiteral("1"));
    }

    // Ctrl+Shift+P's path, with the preview hidden: the page loads in a
    // window that stays hidden, the text just typed is included, and the PDF
    // has the math typeset, on white, inside the page margins.
    void exportsPdfWithRenderedMath() {
        QSettings().setValue(QStringLiteral("preview/visible"), false);
        auto editor = createEditor();
        // Close it even when a check fails, so the next test starts clean.
        const auto cleanup = qScopeGuard([&] {
            closeEditor(editor);
            QSettings().remove(QStringLiteral("preview"));
        });
        QVERIFY(editor.window);
        editor.backend->open(fixtureUrl(QStringLiteral("sample.md")));
        QObject *integration = editor.window->findChild<QObject *>(QStringLiteral("previewIntegration"));
        QObject *text = editor.window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(integration && text);
        // Typed a moment ago: still inside the preview's debounce. Enough
        // of it to run over several pages.
        QString typed = QStringLiteral("\n\nFreshly typed line.\n");
        for (int i = 0; i < 80; ++i)
            typed += QStringLiteral("\nFiller paragraph %1, long enough to be a line of prose.\n").arg(i);
        text->setProperty("text", text->property("text").toString() + typed);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("sample.pdf"));
        QVERIFY(QMetaObject::invokeMethod(
            integration, "requestOutput",
            Q_ARG(QVariant, QVariantMap({{QStringLiteral("kind"), QStringLiteral("pdf")},
                                         {QStringLiteral("url"), QUrl::fromLocalFile(path)}}))));
        QTRY_COMPARE_WITH_TIMEOUT(editor.backend->status(), QStringLiteral("Exported sample.pdf"), 30000);
        QTRY_VERIFY(previewWindow());
        QVERIFY(!previewWindow()->isVisible());
        QCOMPARE(editor.backend->previewVisible(), false);

        QPdfDocument pdf;
        QCOMPARE(pdf.load(path), QPdfDocument::Error::None);
        QVERIFY(pdf.pageCount() >= 3);
        const QSizeF page = pdf.pagePointSize(0);
        QVERIFY(qAbs(page.width() - 595) < 2 || qAbs(page.width() - 612) < 2);
        QString all;
        for (int i = 0; i < pdf.pageCount(); ++i)
            all += pdf.getAllText(i).text();
        QVERIFY(all.contains(QStringLiteral("Heat equation")));
        QVERIFY(all.contains(QStringLiteral("Freshly typed line.")));
        QVERIFY(all.contains(QChar(0x03B1)));            // α, typeset by KaTeX
        QVERIFY(!all.contains(QStringLiteral("$$")));     // no source delimiters
        // Inside 20 mm (57 pt) margins on every page, top and bottom
        // included, on white paper despite the dark theme.
        for (int i = 0; i < pdf.pageCount(); ++i) {
            const QRectF bounds = pdf.getAllText(i).boundingRectangle();
            QVERIFY2(bounds.left() >= 50 && bounds.top() >= 50
                         && bounds.right() <= page.width() - 50
                         && bounds.bottom() <= page.height() - 50,
                     qPrintable(QStringLiteral("page %1: text in %2,%3 to %4,%5").arg(i)
                                    .arg(bounds.left()).arg(bounds.top())
                                    .arg(bounds.right()).arg(bounds.bottom())));
        }
        const QImage image = pdf.render(0, (page * 0.5).toSize());
        QCOMPARE(QColor(image.pixel(5, 5)), QColor(Qt::white));
    }

    // Ctrl+P's path: Backend hands print to the preview, which renders a
    // temporary PDF and prints its pages (to a file here, not a dialog).
    void printsThroughThePreview() {
        auto editor = createEditor();
        // Close it even when a check fails, so the next test starts clean.
        const auto cleanup = qScopeGuard([&] {
            closeEditor(editor);
            QSettings().remove(QStringLiteral("preview"));
        });
        QVERIFY(editor.window);
        editor.backend->open(fixtureUrl(QStringLiteral("math-valid.md")));
        QTRY_VERIFY(previewWindow());
        QTRY_VERIFY_WITH_TIMEOUT(editor.backend->previewBridge()->pageReady(), 20000);
        QObject *sandbox = editor.engine->singletonInstance<QObject *>(
            QStringLiteral("Omalorem.Preview"), QStringLiteral("PreviewSandbox"));
        QVERIFY(sandbox);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString printed = directory.filePath(QStringLiteral("printed.pdf"));
        sandbox->setProperty("printTestTarget", printed);

        editor.backend->printDocument();
        QTRY_COMPARE_WITH_TIMEOUT(editor.backend->status(),
                                  QStringLiteral("Sent math-valid.md to the printer"), 30000);
        sandbox->setProperty("printTestTarget", QString());
        QPdfDocument pdf;
        QCOMPARE(pdf.load(printed), QPdfDocument::Error::None);
        QVERIFY(pdf.pageCount() >= 1);
        // Printed as page images: not blank.
        const QImage image = pdf.render(0, QSize(300, 420));
        bool ink = false;
        for (int y = 0; y < image.height() && !ink; ++y) {
            for (int x = 0; x < image.width() && !ink; ++x)
                ink = QColor(image.pixel(x, y)).lightness() < 128;
        }
        QVERIFY(ink);
    }

    // Ctrl+P through the desktop's print portal (a mock here): the dialog
    // comes first, the page is rendered on the paper chosen there, and the
    // PDF itself goes to the portal with the token it handed out.
    void printsThroughThePrintPortal() {
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                            QStringLiteral("omalorem-mock-portal"));
        if (!bus.isConnected())
            QSKIP("No D-Bus session bus");
        QThread thread;
        thread.start();
        auto *portal = new MockPrintPortal(bus);
        portal->moveToThread(&thread);
        const auto stopPortal = qScopeGuard([&] {
            bus.unregisterService(QString::fromLatin1(MockPrintPortal::service));
            bus.unregisterObject(QStringLiteral("/org/freedesktop/portal/desktop"));
            QMetaObject::invokeMethod(portal, &QObject::deleteLater);
            thread.quit();
            thread.wait();
            QDBusConnection::disconnectFromBus(QStringLiteral("omalorem-mock-portal"));
        });
        QVERIFY(bus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), portal,
                                   QDBusConnection::ExportAllSlots
                                       | QDBusConnection::ExportAllProperties));
        QVERIFY(bus.registerService(QString::fromLatin1(MockPrintPortal::service)));

        auto editor = createEditor();
        const auto cleanup = qScopeGuard([&] {
            closeEditor(editor);
            QSettings().remove(QStringLiteral("preview"));
        });
        QVERIFY(editor.window);
        editor.backend->open(fixtureUrl(QStringLiteral("math-valid.md")));
        QTRY_VERIFY(previewWindow());
        QTRY_VERIFY_WITH_TIMEOUT(editor.backend->previewBridge()->pageReady(), 20000);
        QObject *sandbox = editor.engine->singletonInstance<QObject *>(
            QStringLiteral("Omalorem.Preview"), QStringLiteral("PreviewSandbox"));
        QVERIFY(sandbox);
        sandbox->setProperty("portalService", QString::fromLatin1(MockPrintPortal::service));

        const auto printedPdf = [&](QPdfDocument &pdf, QBuffer &buffer) {
            buffer.setData(portal->printed);
            buffer.open(QIODevice::ReadOnly);
            pdf.load(&buffer);
        };

        // A4 in landscape, chosen in the dialog.
        portal->pageSetup = {{QStringLiteral("PPDName"), QStringLiteral("iso_a4")},
                             {QStringLiteral("Width"), 210.0},
                             {QStringLiteral("Height"), 297.0},
                             {QStringLiteral("Orientation"), QStringLiteral("landscape")}};
        editor.backend->printDocument();
        QTRY_COMPARE_WITH_TIMEOUT(editor.backend->status(),
                                  QStringLiteral("Sent math-valid.md to the printer"), 30000);
        QCOMPARE(portal->printCalls.load(), 1);
        QCOMPARE(portal->printedToken, 42u);
        QVERIFY(portal->printed.startsWith("%PDF"));
        {
            QBuffer buffer;
            QPdfDocument pdf;
            printedPdf(pdf, buffer);
            QVERIFY(pdf.pageCount() >= 1);
            QVERIFY(qAbs(pdf.pagePointSize(0).width() - 842) < 2);
            QVERIFY(qAbs(pdf.pagePointSize(0).height() - 595) < 2);
            // Vector text, not page images.
            QVERIFY(pdf.getAllText(0).text().contains(QStringLiteral("Valid math")));
        }

        // Letter, portrait.
        portal->pageSetup = {{QStringLiteral("Width"), 215.9},
                             {QStringLiteral("Height"), 279.4},
                             {QStringLiteral("Orientation"), QStringLiteral("portrait")}};
        editor.backend->previewReportStatus(QString());
        editor.backend->printDocument();
        QTRY_COMPARE_WITH_TIMEOUT(editor.backend->status(),
                                  QStringLiteral("Sent math-valid.md to the printer"), 30000);
        QCOMPARE(portal->printCalls.load(), 2);
        {
            QBuffer buffer;
            QPdfDocument pdf;
            printedPdf(pdf, buffer);
            QVERIFY(qAbs(pdf.pagePointSize(0).width() - 612) < 2);
            QVERIFY(qAbs(pdf.pagePointSize(0).height() - 792) < 2);
        }

        // Cancelled in the dialog: nothing is rendered or printed.
        portal->prepareResponse = 1;
        editor.backend->printDocument();
        QTRY_COMPARE(editor.backend->status(), QStringLiteral("Print cancelled"));
        QTest::qWait(200);
        QCOMPARE(portal->printCalls.load(), 2);
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

    QVariantMap lastRender() {
        return js("window.omaloremPreview.lastRender").toMap();
    }

    // The performance fixture from spec 5.4: 100 sections of 20 lines, each
    // with one inline and one display equation, so 2,000 lines and 200
    // equations. Built here rather than stored, so it cannot drift.
    static QString longDocument() {
        QString document;
        for (int i = 0; i < 100; ++i) {
            const QString n = QString::number(i);
            document += QStringLiteral(
                "## Section %1\n"
                "\n"
                "The field $E_{%1} = -\\nabla \\phi_{%1}$ follows from the potential.\n"
                "\n"
                "$$\n"
                "\\int_0^{%1} x^2 \\, dx = \\frac{%1^3}{3}\n"
                "$$\n"
                "\n"
                "Prose that runs on for a while, so that the section has some text in it.\n"
                "\n"
                "- first point\n"
                "- second point\n"
                "\n"
                "```\n"
                "code %1\n"
                "```\n"
                "\n"
                "> A quotation to end section %1.\n"
                "\n"
                "\n").arg(n);
        }
        return document;
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
    // No test may reach the desktop's real print portal and open a dialog:
    // PreviewSandbox's default service is replaced by one that doesn't exist.
    qputenv("OMALOREM_PRINT_PORTAL", "org.omalorem.NoPrintPortal");
    // As in the application: WebEngine arrives with the preview plugin and
    // needs context sharing set before the QApplication exists.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    PreviewTest test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_preview.moc"
