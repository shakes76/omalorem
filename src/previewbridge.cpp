#include "previewbridge.h"

#include <QFileInfo>

#include <utility>

#include "backend.h"

PreviewBridge::PreviewBridge(QObject *parent) : QObject(parent) {
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(debounceInterval);
    connect(&m_debounceTimer, &QTimer::timeout, this, [this]() {
        // Take the text first: setMarkdown() clears the pending copy.
        const QString pending = std::exchange(m_pendingMarkdown, QString());
        setMarkdown(pending);
    });
}

void PreviewBridge::scheduleMarkdown(const QString &markdown) {
    m_pendingMarkdown = markdown;
    m_debounceTimer.start();
}

void PreviewBridge::setMarkdown(const QString &markdown) {
    m_debounceTimer.stop();
    m_pendingMarkdown.clear();
    if (m_markdown == markdown)
        return;

    m_markdown = markdown;
    emit markdownChanged();
}

void PreviewBridge::setDocumentUrl(const QUrl &fileUrl) {
    // An unsaved document has no folder, so nothing local may load for it.
    QString baseUrl;
    if (fileUrl.isLocalFile()) {
        const QString folder = QFileInfo(fileUrl.toLocalFile()).absolutePath();
        baseUrl = QUrl::fromLocalFile(folder.endsWith(QLatin1Char('/'))
                                          ? folder
                                          : folder + QLatin1Char('/')).toString();
    }
    if (m_baseUrl == baseUrl)
        return;

    m_baseUrl = baseUrl;
    emit baseUrlChanged();
}

void PreviewBridge::setTheme(const QString &background, const QString &foreground,
                             const QString &accent, const QString &selection, bool dark) {
    const QVariantMap theme{{QStringLiteral("bg"), background},
                            {QStringLiteral("fg"), foreground},
                            {QStringLiteral("accent"), accent},
                            {QStringLiteral("selection"), selection},
                            {QStringLiteral("muted"), mutedColor(dark)},
                            {QStringLiteral("dark"), dark}};
    if (m_theme == theme)
        return;

    m_theme = theme;
    emit themeChanged();
}

void PreviewBridge::setTextScale(qreal textScale) {
    if (qFuzzyCompare(m_textScale, textScale))
        return;

    m_textScale = textScale;
    emit textScaleChanged();
}

void PreviewBridge::setFontFamily(const QString &fontFamily) {
    if (m_fontFamily == fontFamily)
        return;

    m_fontFamily = fontFamily;
    emit fontFamilyChanged();
}

void PreviewBridge::setSourceLine(int sourceLine) {
    if (m_sourceLine == sourceLine)
        return;

    m_sourceLine = sourceLine;
    emit sourceLineChanged();
}

// Mirrors `mutedColor` in Main.qml, which is not themed, so the page's
// secondary text matches the editor's footer and placeholder.
QString PreviewBridge::mutedColor(bool dark) {
    return dark ? QStringLiteral("#909191") : QStringLiteral("#aeb1b5");
}

void PreviewBridge::ready() {
    if (m_pageReady)
        return;

    m_pageReady = true;
    emit pageReadyChanged();
}

void PreviewBridge::openLink(const QString &url) {
    const QUrl target(url);
    if (Backend::isExternalUrlAllowed(target))
        emit externalLinkRequested(target);
}
