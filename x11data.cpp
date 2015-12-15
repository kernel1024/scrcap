#include "x11data.h"

Display* p_display = NULL;

X11Data::X11Data(QObject *parent) : QObject(parent)
{
    if (p_display == NULL)
        p_display = XOpenDisplay(NULL);
    m_display = p_display;
}

bool X11Data::isValid()
{
    return m_display != 0;
}

Display* X11Data::display()
{
    Q_ASSERT(isValid());
    return m_display;
}

Window X11Data::rootWindow()
{
    return DefaultRootWindow(display());
}
