
QT       += core gui widgets x11extras

TARGET = scrcap
TEMPLATE = app

CONFIG += link_pkgconfig c++11

PKGCONFIG += xcb xcb-xfixes xcb-image

SOURCES += main.cpp \
    mainwindow.cpp \
    funcs.cpp \
    windowgrabber.cpp

FORMS += \
    mainwindow.ui

HEADERS += \
    mainwindow.h \
    funcs.h \
    windowgrabber.h
