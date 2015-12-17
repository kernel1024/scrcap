#ifndef XCBTOOLS_H
#define XCBTOOLS_H

#include <QPixmap>
#include <QVector>
#include <QRect>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/xfixes.h>

QPixmap convertFromNative(xcb_image_t *xcbImage);
bool getWindowGeometry(xcb_window_t window, int &x, int &y, int &w, int &h);
QPixmap getWindowPixmap(xcb_window_t window, bool blendPointer);
QPixmap blendCursorImage(const QPixmap &pixmap, int x, int y, int width, int height);
void getWindowsRecursive( QVector<QRect> &windows, xcb_window_t w, int rx = 0, int ry = 0, int depth = 0 );
xcb_window_t findRealWindow( xcb_window_t w, int depth = 0 );
xcb_window_t windowUnderCursor( bool includeDecorations = true );

#endif // XCBTOOLS_H

