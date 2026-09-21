#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QWebEngineUrlRequestInterceptor>

class PreviewBridge;
class QQuickWebEngineProfile;

// Enforces the preview's offline rule: every request the page makes, from
// the page itself to fonts and images, is checked against the bridge's
// policy (qrc: plus the document's folder) and anything else is blocked.
class PreviewRequestInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT

public:
    explicit PreviewRequestInterceptor(PreviewBridge *bridge, QObject *parent = nullptr);

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
    QStringList blockedRequests() const { return m_blockedRequests; }

signals:
    void requestBlocked(const QUrl &url);

private:
    QPointer<PreviewBridge> m_bridge;
    QStringList m_blockedRequests;
};

// Hands the preview's WebEngineView its profile. A WebEngineProfile declared
// in QML cannot take a request interceptor, so the profile is built here, and
// only when the first view asks for it: until then nothing touches Chromium.
class PreviewSandbox : public QObject {
    Q_OBJECT

public:
    explicit PreviewSandbox(PreviewBridge *bridge, QObject *parent = nullptr);

    Q_INVOKABLE QQuickWebEngineProfile *profile();
    Q_INVOKABLE QStringList blockedRequests() const;
    PreviewRequestInterceptor *interceptor() const { return m_interceptor; }

private:
    QPointer<PreviewBridge> m_bridge;
    QQuickWebEngineProfile *m_profile = nullptr;
    PreviewRequestInterceptor *m_interceptor = nullptr;
};
