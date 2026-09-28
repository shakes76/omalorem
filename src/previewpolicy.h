#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QUrl>

// The scheme that serves the document's folder to the preview page, as
// omalorem-doc:/figures/plot.png. Chromium refuses file: URLs to a page loaded
// from qrc:, so local images come through this instead (docs/SPEC.md §4.4).
inline QString previewDocumentScheme() {
    return QStringLiteral("omalorem-doc");
}

// Maps an omalorem-doc: URL to the local file it names inside the document's
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

// A local file inside the document's folder, as a cleaned path, or an empty
// string. `..` segments cannot climb out, and the folder itself is not a file
// in it. This is the containment rule; callers add their own checks.
inline QString previewFolderFilePath(const QUrl &url, const QString &baseUrl) {
    if (baseUrl.isEmpty() || !url.isLocalFile() || !url.host().isEmpty())
        return {};

    const QString folder = QUrl(baseUrl).toLocalFile();
    if (folder.isEmpty())
        return {};
    const QString path = QDir::cleanPath(url.toLocalFile());
    return path.startsWith(folder) && path.size() > folder.size() ? path : QString();
}

inline bool isPreviewMarkdownPath(const QString &path) {
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".md")) || lower.endsWith(QStringLiteral(".markdown"));
}

// The document a preview link points to, or an empty string when the link must
// not be followed (docs/SPEC.md §4.5): a Markdown file that exists inside the
// document's folder or a subfolder, with symlinks resolved, which is the rule
// the images already use.
inline QString previewLinkedDocumentPath(const QUrl &url, const QString &baseUrl) {
    const QString path = previewFolderFilePath(url, baseUrl);
    if (path.isEmpty() || !isPreviewMarkdownPath(path))
        return {};

    const QFileInfo target(path);
    if (!target.isFile())
        return {};
    // A symlink inside the folder may still lead out of it.
    const QString folder = QFileInfo(QUrl(baseUrl).toLocalFile()).canonicalFilePath();
    const QString canonical = target.canonicalFilePath();
    if (folder.isEmpty() || canonical.isEmpty()
        || !canonical.startsWith(folder + QLatin1Char('/')))
        return {};
    return path;
}

// The preview's resource rule, shared by PreviewBridge (main binary) and the
// request interceptor (preview plugin): bundled qrc assets, plus files inside
// the document's folder through omalorem-doc:. Everything else, remote or not
// and file: included, is refused.
inline bool isPreviewResourceAllowed(const QUrl &url, const QString &baseUrl) {
    if (url.scheme().toLower() == QStringLiteral("qrc"))
        return true;
    return !previewDocumentPath(url, baseUrl).isEmpty();
}
