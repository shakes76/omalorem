# The Omaview.Preview QML module: everything that needs QtWebEngine. Built as
# a plugin so the omaview executable never links WebEngine (docs/SPEC.md §5.3).
# omaview.pro builds it into <build>/Omaview/Preview beside the executable.
TEMPLATE = lib
CONFIG += plugin c++17 release
TARGET = omaviewpreviewplugin
QT += qml quick webenginequick webchannel

INCLUDEPATH += $$PWD/..
HEADERS += previewsandbox.h ../previewpolicy.h
SOURCES += previewplugin.cpp previewsandbox.cpp
RESOURCES += previewplugin.qrc

isEmpty(PREVIEW_DESTDIR): PREVIEW_DESTDIR = $$OUT_PWD/Omaview/Preview
DESTDIR = $$PREVIEW_DESTDIR
COPIES += qmldirfile
qmldirfile.files = qmldir
qmldirfile.path = $$DESTDIR

# Installed privately; main.cpp looks in <prefix>/lib/omaview/qml, beside
# <prefix>/bin/omaview. `make install INSTALL_ROOT=$pkgdir` in the plugin's
# build directory puts both files in place.
isEmpty(PREVIEW_INSTALL_DIR): PREVIEW_INSTALL_DIR = /usr/lib/omaview/qml/Omaview/Preview
target.path = $$PREVIEW_INSTALL_DIR
qmldirinstall.files = qmldir
qmldirinstall.path = $$PREVIEW_INSTALL_DIR
INSTALLS += target qmldirinstall
