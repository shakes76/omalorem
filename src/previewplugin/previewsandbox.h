#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QWebEngineUrlRequestInterceptor>

class QQuickWebEngineProfile;

// Enforces the preview's offline rule: every request the page makes, from
// the page itself to fonts and images, is checked against the resource
// policy (qrc: plus the document's folder) and anything else is blocked.
class PreviewRequestInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT

public:
    explicit PreviewRequestInterceptor(QObject *parent = nullptr);

    // The PreviewBridge whose baseUrl names the document's folder. It lives
    // in the executable, so it is reached through its properties only.
    void setBridge(QObject *bridge) { m_bridge = bridge; }
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
    QStringList blockedRequests() const { return m_blockedRequests; }

private:
    QPointer<QObject> m_bridge;
    QStringList m_blockedRequests;
};

// QML singleton `PreviewSandbox` of the Omaview.Preview module. The profile
// comes from a WebEngineProfilePrototype in PreviewPane.qml (a WebEngineProfile
// declared in QML cannot take an interceptor, and Qt 6.9+ asks for prototypes
// over profiles built directly); this adds the interceptor to it.
class PreviewSandbox : public QObject {
    Q_OBJECT

public:
    explicit PreviewSandbox(QObject *parent = nullptr);

    Q_INVOKABLE QQuickWebEngineProfile *protect(QQuickWebEngineProfile *profile,
                                                QObject *bridge);
    Q_INVOKABLE QStringList blockedRequests() const;

private:
    PreviewRequestInterceptor *m_interceptor = nullptr;
};
