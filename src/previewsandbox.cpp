#include "previewsandbox.h"

#include <QLoggingCategory>
#include <QQmlEngine>
#include <QQuickWebEngineProfile>
#include <QWebEngineUrlRequestInfo>

#include "previewbridge.h"

Q_LOGGING_CATEGORY(previewSandboxLog, "omaview.preview.sandbox")

PreviewRequestInterceptor::PreviewRequestInterceptor(PreviewBridge *bridge, QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent), m_bridge(bridge) {}

// Qt 6 calls this on the UI thread, so reading the bridge needs no locking.
void PreviewRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info) {
    const QUrl url = info.requestUrl();
    if (m_bridge && m_bridge->isResourceAllowed(url))
        return;

    info.block(true);
    m_blockedRequests.append(url.toString());
    qCDebug(previewSandboxLog) << "Blocked" << url;
    emit requestBlocked(url);
}

PreviewSandbox::PreviewSandbox(PreviewBridge *bridge, QObject *parent)
    : QObject(parent), m_bridge(bridge) {}

QQuickWebEngineProfile *PreviewSandbox::profile() {
    if (m_profile)
        return m_profile;

    // No storage name makes the profile off the record: no cookies, cache or
    // history ever reach the disk.
    m_profile = new QQuickWebEngineProfile(this);
    m_profile->setHttpCacheType(QQuickWebEngineProfile::MemoryHttpCache);
    m_profile->setPersistentCookiesPolicy(QQuickWebEngineProfile::NoPersistentCookies);
    m_interceptor = new PreviewRequestInterceptor(m_bridge, m_profile);
    m_profile->setUrlRequestInterceptor(m_interceptor);
    // Objects returned to QML from an invokable default to JavaScript
    // ownership; the profile must outlive every view, so keep it ours.
    QQmlEngine::setObjectOwnership(m_profile, QQmlEngine::CppOwnership);
    return m_profile;
}

QStringList PreviewSandbox::blockedRequests() const {
    return m_interceptor ? m_interceptor->blockedRequests() : QStringList();
}
