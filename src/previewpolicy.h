#pragma once

#include <QDir>
#include <QString>
#include <QUrl>

// The scheme that serves the document's folder to the preview page, as
// omaview-doc:/figures/plot.png. Chromium refuses file: URLs to a page loaded
// from qrc:, so local images come through this instead (docs/SPEC.md §4.4).
inline QString previewDocumentScheme() {
    return QStringLiteral("omaview-doc");
}

// Maps an omaview-doc: URL to the local file it names inside the document's
// folder (baseUrl, a file: URL ending in '/'), or returns an empty string if
// it names anything else. Header-only, like the rule below, so the plugin
// needs no symbols from the executable.
inline QString previewDocumentPath(const QUrl &url, const QString &baseUrl) {
    if (url.scheme().toLower() != previewDocumentScheme() || baseUrl.isEmpty()
        || !url.host().isEmpty())
        return {};

    // Compare cleaned paths so `..` segments, even percent-encoded ones,
    // cannot climb out of the folder.
    const QString folder = QUrl(baseUrl).toLocalFile();
    if (folder.isEmpty())
        return {};
    const QString path = QDir::cleanPath(folder + url.path(QUrl::FullyDecoded));
    return path.startsWith(folder) && path.size() > folder.size() ? path : QString();
}

// The preview's resource rule, shared by PreviewBridge (main binary) and the
// request interceptor (preview plugin): bundled qrc assets, plus files inside
// the document's folder through omaview-doc:. Everything else, remote or not
// and file: included, is refused.
inline bool isPreviewResourceAllowed(const QUrl &url, const QString &baseUrl) {
    if (url.scheme().toLower() == QStringLiteral("qrc"))
        return true;
    return !previewDocumentPath(url, baseUrl).isEmpty();
}
