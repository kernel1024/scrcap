
QT       += core gui widgets dbus

TARGET = scrcap
TEMPLATE = app

CONFIG += link_pkgconfig c++17 rtti

PKGCONFIG += xcb xcb-xfixes xcb-image xcb-keysyms

SOURCES += main.cpp \
    gstplayer.cpp \
    mainwindow.cpp \
    funcs.cpp \
    windowgrabber.cpp \
    regiongrabber.cpp \
    xcbtools.cpp \
    qxtglobalshortcut.cpp

FORMS += \
    mainwindow.ui

HEADERS += \
    gstplayer.h \
    mainwindow.h \
    funcs.h \
    windowgrabber.h \
    regiongrabber.h \
    xcbtools.h \
    qxtglobalshortcut.h

packagesExist(gstreamer-1.0) {
    PKGCONFIG += gstreamer-1.0
    CONFIG += use_gst
    DEFINES += WITH_GST=1
    message("GStreamer support: YES")
}

!use_gst {
    message("GStreamer support: NO")
}

DISTFILES += \
    README.md
