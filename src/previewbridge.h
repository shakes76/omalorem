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
    // Counts markdown changes. The page reports back the revision it has
    // finished rendering (fonts and images included), so PDF export and
    // print can wait until the page shows the text as it is now.
    Q_PROPERTY(int markdownRevision READ markdownRevision NOTIFY markdownChanged)
    Q_PROPERTY(int renderedRevision READ renderedRevision NOTIFY renderedRevisionChanged)
    Q_PROPERTY(bool pageReady READ pageReady NOTIFY pageReadyChanged)
    Q_PROPERTY(QString baseUrl READ baseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(qreal textScale READ textScale NOTIFY textScaleChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY fontFamilyChanged)
    // The source line at the top of the editor's view, plus the fraction of
    // it scrolled past, which the preview lines up with (editor → preview).
    Q_PROPERTY(qreal sourceLine READ sourceLine WRITE setSourceLine NOTIFY sourceLineChanged)

public:
    // Matches the word-count timer, so a pause in typing updates both at once.
    static constexpr int debounceInterval = 120;

    explicit PreviewBridge(QObject *parent = nullptr);

    QString markdown() const { return m_markdown; }
    int markdownRevision() const { return m_markdownRevision; }
    int renderedRevision() const { return m_renderedRevision; }
    // Whether the page shows the current markdown, settled.
    bool renderedCurrent() const { return m_pageReady && m_renderedRevision == m_markdownRevision; }
    QString baseUrl() const { return m_baseUrl; }
    QVariantMap theme() const { return m_theme; }
    qreal textScale() const { return m_textScale; }
    QString fontFamily() const { return m_fontFamily; }
    qreal sourceLine() const { return m_sourceLine; }
    bool pageReady() const { return m_pageReady; }

    // Typing goes through the debounce; opening or reloading a file does not.
    void scheduleMarkdown(const QString &markdown);
    void setMarkdown(const QString &markdown);
    void setDocumentUrl(const QUrl &fileUrl);
    void setTheme(const QString &background, const QString &foreground,
                  const QString &accent, const QString &selection, bool dark);
    void setTextScale(qreal textScale);
    void setFontFamily(const QString &fontFamily);
    void setSourceLine(qreal sourceLine);

    static QString mutedColor(bool dark);
    // The request interceptor's policy: bundled qrc assets, plus local files
    // inside the document's folder. Everything else, remote or not, is refused.
    bool isResourceAllowed(const QUrl &url) const;
    static bool isExternalLinkAllowed(const QUrl &url);

    Q_INVOKABLE void ready();
    Q_INVOKABLE void openLink(const QString &url);
    // Called by the page once a render has settled.
    Q_INVOKABLE void rendered(int revision);

signals:
    void markdownChanged();
    void baseUrlChanged();
    void themeChanged();
    void textScaleChanged();
    void fontFamilyChanged();
    void sourceLineChanged();
    void pageReadyChanged();
    void renderedRevisionChanged();
    // Only emitted for URLs that pass Backend's external-link filter.
    void externalLinkRequested(const QUrl &url);

private:
    QString m_markdown;
    QString m_pendingMarkdown;
    int m_markdownRevision = 0;
    int m_renderedRevision = -1;
    QString m_baseUrl;
    QVariantMap m_theme;
    qreal m_textScale = 1.0;
    QString m_fontFamily = QStringLiteral("mono");
    qreal m_sourceLine = 0;
    bool m_pageReady = false;
    QTimer m_debounceTimer;
};
