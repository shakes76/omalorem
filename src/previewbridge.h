#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

// The preview page's view of one editor: everything it needs to render,
// published over a QWebChannel. It deliberately knows nothing about
// QtWebEngine, so the editor (and its tests) can build without Chromium.
class PreviewBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString markdown READ markdown NOTIFY markdownChanged)
    Q_PROPERTY(QString baseUrl READ baseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(qreal textScale READ textScale NOTIFY textScaleChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(int sourceLine READ sourceLine WRITE setSourceLine NOTIFY sourceLineChanged)

public:
    // Matches the word-count timer, so a pause in typing updates both at once.
    static constexpr int debounceInterval = 120;

    explicit PreviewBridge(QObject *parent = nullptr);

    QString markdown() const { return m_markdown; }
    QString baseUrl() const { return m_baseUrl; }
    QVariantMap theme() const { return m_theme; }
    qreal textScale() const { return m_textScale; }
    QString fontFamily() const { return m_fontFamily; }
    int sourceLine() const { return m_sourceLine; }
    bool pageReady() const { return m_pageReady; }

    // Typing goes through the debounce; opening or reloading a file does not.
    void scheduleMarkdown(const QString &markdown);
    void setMarkdown(const QString &markdown);
    void setDocumentUrl(const QUrl &fileUrl);
    void setTheme(const QString &background, const QString &foreground,
                  const QString &accent, const QString &selection, bool dark);
    void setTextScale(qreal textScale);
    void setFontFamily(const QString &fontFamily);
    void setSourceLine(int sourceLine);

    static QString mutedColor(bool dark);
    // The request interceptor's policy: bundled qrc assets, plus local files
    // inside the document's folder. Everything else, remote or not, is refused.
    bool isResourceAllowed(const QUrl &url) const;

    Q_INVOKABLE void ready();
    Q_INVOKABLE void openLink(const QString &url);

signals:
    void markdownChanged();
    void baseUrlChanged();
    void themeChanged();
    void textScaleChanged();
    void fontFamilyChanged();
    void sourceLineChanged();
    void pageReadyChanged();
    // Only emitted for URLs that pass Backend's external-link filter.
    void externalLinkRequested(const QUrl &url);

private:
    QString m_markdown;
    QString m_pendingMarkdown;
    QString m_baseUrl;
    QVariantMap m_theme;
    qreal m_textScale = 1.0;
    QString m_fontFamily = QStringLiteral("mono");
    int m_sourceLine = 0;
    bool m_pageReady = false;
    QTimer m_debounceTimer;
};
