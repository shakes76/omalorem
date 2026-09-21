#include "previewsandbox.h"

#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QQmlEngine>
#include <QQuickWebEngineProfile>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>

#include "previewpolicy.h"

Q_LOGGING_CATEGORY(previewSandboxLog, "omalorem.preview.sandbox")

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
