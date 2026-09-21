#include "previewbridge.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>

#include <utility>

#include "previewpolicy.h"


// Off by default; QT_LOGGING_RULES="omalorem.preview.info=true" shows when the
// page first renders, which is how the startup budget in spec 5.3 is measured.
Q_LOGGING_CATEGORY(previewLog, "omalorem.preview", QtWarningMsg)

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
    ++m_markdownRevision;
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

void PreviewBridge::setSourceLine(qreal sourceLine) {
    // A thousandth of a line is well under a pixel; finer changes aren't
    // worth a trip over the channel.
    if (qAbs(m_sourceLine - sourceLine) < 0.001)
        return;

    m_sourceLine = sourceLine;
    emit sourceLineChanged();
}

// Mirrors `mutedColor` in Main.qml, which is not themed, so the page's
// secondary text matches the editor's footer and placeholder.
QString PreviewBridge::mutedColor(bool dark) {
    return dark ? QStringLiteral("#909191") : QStringLiteral("#aeb1b5");
}

bool PreviewBridge::isResourceAllowed(const QUrl &url) const {
    return isPreviewResourceAllowed(url, m_baseUrl);
}

void PreviewBridge::ready() {
    if (m_pageReady)
        return;

    m_pageReady = true;
    qCInfo(previewLog) << "Preview page rendered";
    emit pageReadyChanged();
}

// The same rule as Backend::openExternalUrl, checked here as well so the page
// can never hand the desktop anything but web and mail links.
bool PreviewBridge::isExternalLinkAllowed(const QUrl &url) {
    const QString scheme = url.scheme().toLower();
    return scheme == QStringLiteral("http") || scheme == QStringLiteral("https")
        || scheme == QStringLiteral("mailto");
}

void PreviewBridge::openLink(const QString &url) {
    const QUrl target(url);
    if (isExternalLinkAllowed(target))
        emit externalLinkRequested(target);
}

void PreviewBridge::rendered(int revision) {
    if (m_renderedRevision == revision)
        return;

    m_renderedRevision = revision;
    emit renderedRevisionChanged();
}
