QT += core gui quick testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_omalorem

INCLUDEPATH += ../src
SOURCES += \
    tst_omalorem.cpp \
    ../src/backend.cpp \
    ../src/markdownhighlighter.cpp
HEADERS += \
    ../src/backend.h \
    ../src/markdownhighlighter.h

QT += widgets printsupport quickcontrols2 quickdialogs2 dbus

# Omalorem wide column: the column arithmetic, header-only.
HEADERS += ../src/editorlayout.h

# Omalorem preview: the bridge has no WebEngine dependency.
SOURCES += ../src/previewbridge.cpp
HEADERS += ../src/previewbridge.h
