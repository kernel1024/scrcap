
QT       += core gui widgets

TARGET = scrcap
TEMPLATE = app

CONFIG += exceptions \
    rtti \
    stl \
    c++11

SOURCES += main.cpp \
    mainwindow.cpp \
    x11data.cpp \
    funcs.cpp

FORMS += \
    mainwindow.ui

HEADERS += \
    mainwindow.h \
    x11data.h \
    funcs.h

exists( /usr/include/X11/extensions/Xfixes.h ) {
    DEFINES += HAVE_X11_EXTENSIONS_XFIXES
    LIBS += -lXfixes
}

LIBS += -lX11
