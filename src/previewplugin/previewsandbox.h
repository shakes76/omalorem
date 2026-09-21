#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantMap>
#include <functional>
#include <QTemporaryDir>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlSchemeHandler>
#include <QWindow>

class QPrinter;
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

// Serves omalorem-doc: requests from the document's folder: image files only,
// and only if the file, with any symlinks resolved, is inside the folder.
class PreviewDocumentSchemeHandler : public QWebEngineUrlSchemeHandler {
    Q_OBJECT

public:
    explicit PreviewDocumentSchemeHandler(QObject *parent = nullptr);

    // Registers the scheme with QtWebEngine. It must happen before the
    // first profile exists, so the plugin does it as it loads.
    static void registerScheme();

    void setBridge(QObject *bridge) { m_bridge = bridge; }
    void requestStarted(QWebEngineUrlRequestJob *job) override;

private:
    QPointer<QObject> m_bridge;
};

// QML type `PreviewExposeWatcher`: emits reexposed() when its window is
// exposed again after the window system stopped showing it, for example
// on returning to a Hyprland workspace. QML has no expose signal of its own.
class PreviewExposeWatcher : public QObject {
    Q_OBJECT
    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)

public:
    explicit PreviewExposeWatcher(QObject *parent = nullptr);

    QWindow *window() const { return m_window; }
    void setWindow(QWindow *window);

signals:
    void windowChanged();
    void reexposed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPointer<QWindow> m_window;
    bool m_exposed = false;
};

// One pending xdg-desktop-portal request: its Response signal arrives on a
// Request object, and this hands the result to a callback, once.
class PreviewPortalRequest : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(uint response, const QVariantMap &results)>;
    PreviewPortalRequest(Callback callback, QObject *parent)
        : QObject(parent), m_callback(std::move(callback)) {}

public slots:
    void response(uint response, const QVariantMap &results);

private:
    Callback m_callback;
};

// QML singleton `PreviewSandbox` of the Omalorem.Preview module. The profile
// comes from a WebEngineProfilePrototype in PreviewPane.qml (a WebEngineProfile
// declared in QML cannot take an interceptor, and Qt 6.9+ asks for prototypes
// over profiles built directly); this adds the interceptor to it, and the
// handler for the document folder's scheme.
class PreviewSandbox : public QObject {
    Q_OBJECT
    // Tests only: when set, printPdf() prints into this PDF file instead of
    // showing the print dialog, so the print path can run unattended.
    Q_PROPERTY(QString printTestTarget MEMBER m_printTestTarget)
    // The desktop portal that prints (org.freedesktop.portal.Print). Tests
    // point it at a mock service; see the constructor for the default.
    Q_PROPERTY(QString portalService MEMBER m_portalService)

public:
    explicit PreviewSandbox(QObject *parent = nullptr);

    Q_INVOKABLE QQuickWebEngineProfile *protect(QQuickWebEngineProfile *profile,
                                                QObject *bridge);
    Q_INVOKABLE QStringList blockedRequests() const;

    // --- PDF export and print (docs/SPEC.md §5.5) ---
    Q_INVOKABLE QString localPath(const QUrl &url) const { return url.toLocalFile(); }
    // A fresh file name for the PDF that Ctrl+P prints, in a private
    // directory removed when the app quits.
    Q_INVOKABLE QString temporaryPdfPath();
    Q_INVOKABLE void removeTemporaryPdf(const QString &path);
    // Shows the print dialog and prints the pages of a PDF, drawn as images:
    // Qt can't send a PDF to a printer as it is. Returns whether it printed.
    Q_INVOKABLE bool printPdf(const QString &pdfPath, const QString &title, QWindow *parent);
    static bool printPdfPages(const QString &pdfPath, QPrinter *printer);

    // Printing through the desktop's print portal: its dialog opens at once
    // (Qt's asks CUPS for printers first, which takes seconds), and it takes
    // the PDF itself, so the output stays vector. Both calls are
    // asynchronous; the results arrive as the signals below, with response
    // 0 for done, 1 for cancelled and 2 for failed.
    Q_INVOKABLE bool portalPrintAvailable();
    Q_INVOKABLE void preparePortalPrint(const QString &title);
    // Prints and then removes a PDF from temporaryPdfPath().
    Q_INVOKABLE void portalPrint(const QString &pdfPath, uint token, const QString &title);
    // The chosen paper as the page must be rendered: sizeId (a QPageSize id,
    // which is also WebEngineView's page size id), widthMm and landscape.
    static QVariantMap paperFromPageSetup(const QVariantMap &pageSetup);

signals:
    void portalPrintPrepared(int response, uint token, const QVariantMap &paper);
    void portalPrintFinished(int response);

private:
    QString m_printTestTarget;
    QString m_portalService;
    int m_portalAvailable = -1;
    int m_portalRequests = 0;
    QTemporaryDir m_printDirectory;
    int m_printCount = 0;

    PreviewRequestInterceptor *m_interceptor = nullptr;
    PreviewDocumentSchemeHandler *m_documentHandler = nullptr;
};
