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

# Omalorem's suggested-name rules, header-only.
HEADERS += ../src/documentname.h

# Omalorem spell check (docs/SPEC.md §4.10): the editor's, so it builds under
# no_preview too. libhunspell costs about 1.4 ms of start-up, so it is linked
# normally; the dictionary it reads stays lazy.
HEADERS += ../src/spellwords.h
HEADERS += ../src/spellchecker.h
SOURCES += ../src/spellchecker.cpp
LIBS += -lhunspell

# Omalorem preview: the bridge has no WebEngine dependency.
SOURCES += ../src/previewbridge.cpp
HEADERS += ../src/previewbridge.h
