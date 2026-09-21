# Render tests for the preview page: a real QtWebEngine view, offscreen.
QT += core gui widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus \
      webenginequick webchannel testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_preview

INCLUDEPATH += ../../src
SOURCES += \
    tst_preview.cpp \
    ../../src/backend.cpp \
    ../../src/markdownhighlighter.cpp \
    ../../src/previewbridge.cpp \
    ../../src/previewsandbox.cpp
HEADERS += \
    ../../src/backend.h \
    ../../src/markdownhighlighter.h \
    ../../src/previewbridge.h \
    ../../src/previewsandbox.h

RESOURCES += ../../src/resources.qrc ../../src/preview.qrc harness.qrc
DEFINES += FIXTURES_DIR=\\\"$$PWD/../fixtures\\\"
