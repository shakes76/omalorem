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
    QT += webenginequick webchannel
    HEADERS += src/previewsandbox.h
    SOURCES += src/previewsandbox.cpp
    RESOURCES += src/preview.qrc
}
