#include "previewsandbox.h"

#include <QLoggingCategory>
#include <QQmlEngine>
#include <QQuickWebEngineProfile>
#include <QWebEngineUrlRequestInfo>

#include "previewpolicy.h"

Q_LOGGING_CATEGORY(previewSandboxLog, "omaview.preview.sandbox")

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

PreviewSandbox::PreviewSandbox(QObject *parent)
    : QObject(parent), m_interceptor(new PreviewRequestInterceptor(this)) {}

QQuickWebEngineProfile *PreviewSandbox::protect(QQuickWebEngineProfile *profile,
                                                QObject *bridge) {
    if (!profile)
        return nullptr;

    m_interceptor->setBridge(bridge);
    profile->setUrlRequestInterceptor(m_interceptor);
    // Objects returned to QML from an invokable default to JavaScript
    // ownership; the profile belongs to its prototype, never to the GC.
    QQmlEngine::setObjectOwnership(profile, QQmlEngine::CppOwnership);
    return profile;
}

QStringList PreviewSandbox::blockedRequests() const {
    return m_interceptor->blockedRequests();
}
