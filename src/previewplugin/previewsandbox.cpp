#include "previewsandbox.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QPainter>
#include <QPdfDocument>
#include <QPrintDialog>
#include <QPrinter>
#include <QQmlEngine>
#include <QQuickWebEngineProfile>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWindow>

#include "previewpolicy.h"

Q_LOGGING_CATEGORY(previewSandboxLog, "omalorem.preview.sandbox")
// QT_LOGGING_RULES="omalorem.preview.expose.debug=true" shows which window
// events arrive, for diagnosing a view left black after a workspace switch.
Q_LOGGING_CATEGORY(previewExposeLog, "omalorem.preview.expose", QtWarningMsg)

PreviewExposeWatcher::PreviewExposeWatcher(QObject *parent) : QObject(parent) {}

void PreviewExposeWatcher::setWindow(QWindow *window) {
    if (m_window == window)
        return;

    if (m_window)
        m_window->removeEventFilter(this);
    m_window = window;
    m_exposed = window && window->isExposed();
    if (m_window)
        m_window->installEventFilter(this);
    emit windowChanged();
}

bool PreviewExposeWatcher::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_window && event->type() == QEvent::Expose) {
        const bool exposed = m_window->isExposed();
        qCDebug(previewExposeLog) << "Expose, exposed:" << exposed;
        if (exposed && !m_exposed)
            emit reexposed();
        m_exposed = exposed;
    }
    return QObject::eventFilter(watched, event);
}

PreviewRequestInterceptor::PreviewRequestInterceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent) {}

// Qt 6 calls this on the UI thread, so reading the bridge needs no locking.
void PreviewRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info) {
    const QUrl url = info.requestUrl();
    const QString baseUrl = m_bridge ? m_bridge->property("baseUrl").toString() : QString();
    if (isPreviewResourceAllowed(url, baseUrl))
        return;

    info.block(true);
    m_blockedRequests.append(url.toString());
    qCDebug(previewSandboxLog) << "Blocked" << url;
}

PreviewDocumentSchemeHandler::PreviewDocumentSchemeHandler(QObject *parent)
    : QWebEngineUrlSchemeHandler(parent) {}

void PreviewDocumentSchemeHandler::registerScheme() {
    const QByteArray name = previewDocumentScheme().toUtf8();
    if (QWebEngineUrlScheme::schemeByName(name).name() == name)
        return;

    // Like qrc: a path, no host. Secure, so the page needs no mixed-content
    // exception. Not LocalScheme: Chromium doesn't count qrc: as local, so
    // that would lock the preview page itself out. The handler is installed
    // only on the preview's own profile, which never shows another page.
    QWebEngineUrlScheme scheme(name);
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Path);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme);
    QWebEngineUrlScheme::registerScheme(scheme);
}

void PreviewDocumentSchemeHandler::requestStarted(QWebEngineUrlRequestJob *job) {
    const QString baseUrl = m_bridge ? m_bridge->property("baseUrl").toString() : QString();
    const QString path = previewDocumentPath(job->requestUrl(), baseUrl);
    if (job->requestMethod() != QByteArrayLiteral("GET") || path.isEmpty()) {
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
    }

    // The folder check above is on the path as written; a symlink inside
    // the folder may still point out of it, so check where it really leads.
    const QString folder = QFileInfo(QUrl(baseUrl).toLocalFile()).canonicalFilePath();
    const QString target = QFileInfo(path).canonicalFilePath();
    if (folder.isEmpty() || target.isEmpty()) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    if (!target.startsWith(folder + QLatin1Char('/'))) {
        qCDebug(previewSandboxLog) << "Refused a link out of the document folder" << path;
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
    }

    // Images only: the page asks for nothing else, and a document should
    // not be able to read the folder's other files into the page.
    const QMimeType type = QMimeDatabase().mimeTypeForFile(target);
    auto *file = new QFile(target, job);
    if (!type.name().startsWith(QStringLiteral("image/")) || !file->open(QIODevice::ReadOnly)) {
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
    }
    job->reply(type.name().toUtf8(), file);
}

PreviewSandbox::PreviewSandbox(QObject *parent)
    : QObject(parent), m_interceptor(new PreviewRequestInterceptor(this)),
      m_documentHandler(new PreviewDocumentSchemeHandler(this)) {}

QQuickWebEngineProfile *PreviewSandbox::protect(QQuickWebEngineProfile *profile,
                                                QObject *bridge) {
    if (!profile)
        return nullptr;

    m_interceptor->setBridge(bridge);
    profile->setUrlRequestInterceptor(m_interceptor);
    m_documentHandler->setBridge(bridge);
    const QByteArray scheme = previewDocumentScheme().toUtf8();
    if (!profile->urlSchemeHandler(scheme))
        profile->installUrlSchemeHandler(scheme, m_documentHandler);
    // Objects returned to QML from an invokable default to JavaScript
    // ownership; the profile belongs to its prototype, never to the GC.
    QQmlEngine::setObjectOwnership(profile, QQmlEngine::CppOwnership);
    return profile;
}

QStringList PreviewSandbox::blockedRequests() const {
    return m_interceptor->blockedRequests();
}

QString PreviewSandbox::temporaryPdfPath() {
    if (!m_printDirectory.isValid())
        return {};
    return m_printDirectory.filePath(QStringLiteral("print-%1.pdf").arg(++m_printCount));
}

void PreviewSandbox::removeTemporaryPdf(const QString &path) {
    // Only ever a file this object handed out.
    if (m_printDirectory.isValid()
        && QFileInfo(path).absolutePath() == QDir(m_printDirectory.path()).absolutePath())
        QFile::remove(path);
}

bool PreviewSandbox::printPdf(const QString &pdfPath, const QString &title, QWindow *parent) {
    QPrinter printer(QPrinter::HighResolution);
    if (!m_printTestTarget.isEmpty()) {
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(m_printTestTarget);
        return printPdfPages(pdfPath, &printer);
    }

    // As Omawrite's own print: the dialog belongs to the editor window.
    QPdfDocument document;
    document.load(pdfPath);
    QPrintDialog dialog(&printer);
    dialog.setWindowTitle(title);
    if (document.pageCount() > 0)
        dialog.setMinMax(1, document.pageCount());
    dialog.winId();
    if (dialog.windowHandle() && parent)
        dialog.windowHandle()->setTransientParent(parent);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    return printPdfPages(pdfPath, &printer);
}

bool PreviewSandbox::printPdfPages(const QString &pdfPath, QPrinter *printer) {
    QPdfDocument document;
    if (document.load(pdfPath) != QPdfDocument::Error::None || document.pageCount() == 0)
        return false;

    QList<int> pages;
    const QPageRanges ranges = printer->pageRanges();
    for (int page = 0; page < document.pageCount(); ++page) {
        if (ranges.isEmpty() || ranges.contains(page + 1))
            pages.append(page);
    }
    if (pages.isEmpty())
        return false;

    // The PDF already has its margins; use the whole sheet. 300 dpi is
    // print quality, and keeps an A4 page image near 35 MB.
    printer->setFullPage(true);
    QPainter painter;
    if (!painter.begin(printer))
        return false;
    const qreal dpi = qMin(300, printer->resolution());
    bool first = true;
    for (const int page : std::as_const(pages)) {
        if (!first)
            printer->newPage();
        first = false;
        const QSizeF points = document.pagePointSize(page);
        const QImage image = document.render(page, (points * dpi / 72.0).toSize());
        // Fit the page to the sheet, centred, in case the paper differs.
        const QRect sheet = painter.viewport();
        const QSize fitted = image.size().scaled(sheet.size(), Qt::KeepAspectRatio);
        painter.setViewport(sheet);
        painter.setWindow(sheet);
        painter.drawImage(QRect(QPoint((sheet.width() - fitted.width()) / 2,
                                       (sheet.height() - fitted.height()) / 2), fitted),
                          image);
    }
    return painter.end();
}
