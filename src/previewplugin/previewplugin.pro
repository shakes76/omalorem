# The Omalorem.Preview QML module: everything that needs QtWebEngine. Built as
# a plugin so the omalorem executable never links WebEngine (docs/SPEC.md §5.3).
# omalorem.pro builds it into <build>/Omalorem/Preview beside the executable.
TEMPLATE = lib
CONFIG += plugin c++17 release
TARGET = omalorempreviewplugin
QT += qml quick webenginequick webchannel
# PDF export and print (docs/SPEC.md §5.5).
QT += pdf printsupport widgets
# Printing through xdg-desktop-portal.
QT += dbus

INCLUDEPATH += $$PWD/..
HEADERS += previewsandbox.h ../previewpolicy.h
SOURCES += previewplugin.cpp previewsandbox.cpp
RESOURCES += previewplugin.qrc

isEmpty(PREVIEW_DESTDIR): PREVIEW_DESTDIR = $$OUT_PWD/Omalorem/Preview
DESTDIR = $$PREVIEW_DESTDIR
COPIES += qmldirfile
qmldirfile.files = qmldir
qmldirfile.path = $$DESTDIR

# Installed privately; main.cpp looks in <prefix>/lib/omalorem/qml, beside
# <prefix>/bin/omalorem. `make install INSTALL_ROOT=$pkgdir` in the plugin's
# build directory puts both files in place.
isEmpty(PREVIEW_INSTALL_DIR): PREVIEW_INSTALL_DIR = /usr/lib/omalorem/qml/Omalorem/Preview
target.path = $$PREVIEW_INSTALL_DIR
qmldirinstall.files = qmldir
qmldirinstall.path = $$PREVIEW_INSTALL_DIR
INSTALLS += target qmldirinstall
