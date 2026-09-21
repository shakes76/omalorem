QT += core gui quick testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_omaview

INCLUDEPATH += ../src
SOURCES += \
    tst_omaview.cpp \
    ../src/backend.cpp \
    ../src/markdownhighlighter.cpp \
    ../src/previewbridge.cpp
HEADERS += \
    ../src/backend.h \
    ../src/markdownhighlighter.h \
    ../src/previewbridge.h

QT += widgets printsupport quickcontrols2 quickdialogs2 dbus
