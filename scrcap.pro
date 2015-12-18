
QT       += core gui widgets x11extras

TARGET = scrcap
TEMPLATE = app

CONFIG += link_pkgconfig c++11

PKGCONFIG += xcb xcb-xfixes xcb-image xcb-keysyms

SOURCES += main.cpp \
    mainwindow.cpp \
    funcs.cpp \
    windowgrabber.cpp \
    regiongrabber.cpp \
    xcbtools.cpp \
    qxtglobalshortcut_x11.cpp \
    qxtglobalshortcut.cpp

FORMS += \
    mainwindow.ui

HEADERS += \
    mainwindow.h \
    funcs.h \
    windowgrabber.h \
    regiongrabber.h \
    xcbtools.h \
    qxtglobalshortcut.h

DISTFILES += \
    README.md
