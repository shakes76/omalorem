#pragma once

#include <QDir>
#include <QString>
#include <QUrl>

// The preview's resource rule, shared by PreviewBridge (main binary) and the
// request interceptor (preview plugin): bundled qrc assets, plus local files
// inside the document's folder. Everything else, remote or not, is refused.
// Header-only, so the plugin needs no symbols from the executable.
inline bool isPreviewResourceAllowed(const QUrl &url, const QString &baseUrl) {
    const QString scheme = url.scheme().toLower();
    if (scheme == QStringLiteral("qrc"))
        return true;
    if (scheme != QStringLiteral("file") || baseUrl.isEmpty() || !url.host().isEmpty())
        return false;

    // Compare cleaned paths so `..` segments cannot climb out of the folder.
    const QString folder = QUrl(baseUrl).toLocalFile();
    const QString path = QDir::cleanPath(url.toLocalFile());
    return path.startsWith(folder) && path.size() > folder.size();
}
