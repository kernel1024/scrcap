#ifndef QXTX11DATA_H
#define QXTX11DATA_H

#include <QObject>
#include <X11/Xlib.h>

class X11Data : public QObject
{
    Q_OBJECT
public:
    explicit X11Data(QObject *parent = 0);
    bool isValid();
    Display* display();
    Window rootWindow();

private:
    Display* m_display;

};

#endif // QXTX11DATA_H
