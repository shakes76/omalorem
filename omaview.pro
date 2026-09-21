QT += core gui widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus

CONFIG += c++17 release
TARGET = omaview
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/markdownhighlighter.h \
    src/systemtheme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/markdownhighlighter.cpp \
    src/systemtheme.cpp

RESOURCES += src/resources.qrc

# --- Omaview preview --------------------------------------------------------
HEADERS += src/previewbridge.h
SOURCES += src/previewbridge.cpp

# `qmake6 CONFIG+=no_preview` (or bin/build-no-preview) builds the plain
# editor: no QtWebEngine, no WebChannel, no web assets. Useful for debugging
# and for rebasing on upstream.
no_preview {
    DEFINES += OMAVIEW_NO_PREVIEW
} else {
    # The executable never links QtWebEngine: the preview is the
    # Omaview.Preview QML plugin, loaded on first show (docs/SPEC.md §5.3).
    # It is built here into Omaview/Preview beside the executable, which is
    # where main.cpp looks during development.
    HEADERS += src/previewpolicy.h
    RESOURCES += src/previewhost.qrc
    previewplugin.target = preview-plugin
    previewplugin.commands = \
        $(MKDIR) $$shell_quote($$OUT_PWD/preview-plugin) && \
        cd $$shell_quote($$OUT_PWD/preview-plugin) && \
        (test -f Makefile || $$QMAKE_QMAKE $$shell_quote($$PWD/src/previewplugin/previewplugin.pro) \
            PREVIEW_DESTDIR=$$shell_quote($$OUT_PWD/Omaview/Preview)) && \
        $(MAKE)
    previewplugin.depends = FORCE
    QMAKE_EXTRA_TARGETS += previewplugin
    PRE_TARGETDEPS += preview-plugin
}
