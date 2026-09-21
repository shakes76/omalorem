# Render and integration tests for the preview: a real QtWebEngine view,
# offscreen. Like the application, the test binary does not link QtWebEngine;
# it loads the Omalorem.Preview plugin, built here into Omalorem/Preview.
QT += core gui widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus testlib
# PDF export tests read the output back.
QT += pdf
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_preview

INCLUDEPATH += ../../src
SOURCES += \
    tst_preview.cpp \
    ../../src/backend.cpp \
    ../../src/markdownhighlighter.cpp \
    ../../src/previewbridge.cpp
HEADERS += \
    ../../src/backend.h \
    ../../src/markdownhighlighter.h \
    ../../src/previewbridge.h

RESOURCES += ../../src/resources.qrc ../../src/previewhost.qrc harness.qrc
DEFINES += FIXTURES_DIR=\\\"$$PWD/../fixtures\\\"

previewplugin.target = preview-plugin
previewplugin.commands = \
    $(MKDIR) $$shell_quote($$OUT_PWD/preview-plugin) && \
    cd $$shell_quote($$OUT_PWD/preview-plugin) && \
    (test -f Makefile || $$QMAKE_QMAKE $$shell_quote($$PWD/../../src/previewplugin/previewplugin.pro) \
        PREVIEW_DESTDIR=$$shell_quote($$OUT_PWD/Omalorem/Preview)) && \
    $(MAKE)
previewplugin.depends = FORCE
QMAKE_EXTRA_TARGETS += previewplugin
PRE_TARGETDEPS += preview-plugin
