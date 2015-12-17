/*
  Copyright (C) 2004 Bernd Brandstetter <bbrand@freenet.de>
  Copyright (C) 2010, 2011 Pau Garcia i Quiles <pgquiles@elpauer.org>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or ( at your option ) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this library; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include "windowgrabber.h"

#include <QBitmap>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QX11Info>
#include <QDebug>

#include <xcb/xfixes.h>

static
const int minSize = 8;

QPixmap convertFromNative(xcb_image_t *xcbImage)
{
    QImage::Format format = QImage::Format_Invalid;

    switch (xcbImage->depth) {
    case 1:
        format = QImage::Format_MonoLSB;
        break;
    case 16:
        format = QImage::Format_RGB16;
        break;
    case 24:
        format = QImage::Format_RGB32;
        break;
    case 30: {
        // Qt doesn't have a matching image format. We need to convert manually
        quint32 *pixels = reinterpret_cast<quint32 *>(xcbImage->data);
        for (uint i = 0; i < (xcbImage->size / 4); i++) {
            int r = (pixels[i] >> 22) & 0xff;
            int g = (pixels[i] >> 12) & 0xff;
            int b = (pixels[i] >>  2) & 0xff;

            pixels[i] = qRgba(r, g, b, 0xff);
        }
        // fall through, Qt format is still Format_ARGB32_Premultiplied
    }
    case 32:
        format = QImage::Format_ARGB32_Premultiplied;
        break;
    default:
        return QPixmap(); // we don't know
    }

    QImage image(xcbImage->data, xcbImage->width, xcbImage->height, format);

    if (image.isNull()) {
        return QPixmap();
    }

    // work around an abort in QImage::color

    if (image.format() == QImage::Format_MonoLSB) {
        image.setColorCount(2);
        image.setColor(0, QColor(Qt::white).rgb());
        image.setColor(1, QColor(Qt::black).rgb());
    }

    // done

    return QPixmap::fromImage(image);
}

bool getWindowGeometry(xcb_window_t window, int &x, int &y, int &w, int &h)
{
    xcb_connection_t *xcbConn = QX11Info::connection();

    xcb_get_geometry_cookie_t geomCookie = xcb_get_geometry_unchecked(xcbConn, window);
    xcb_get_geometry_reply_t* geomReply = xcb_get_geometry_reply(xcbConn, geomCookie, NULL);

    if (geomReply) {
        x = geomReply->x;
        y = geomReply->y;
        w = geomReply->width;
        h = geomReply->height;
        free(geomReply);
        return true;
    } else {
        x = 0;
        y = 0;
        w = 0;
        h = 0;
        return false;
    }
}

QPixmap getWindowPixmap(xcb_window_t window, bool blendPointer)
{
    xcb_connection_t *xcbConn = QX11Info::connection();

    // first get geometry information for our drawable

    xcb_get_geometry_cookie_t geomCookie = xcb_get_geometry_unchecked(xcbConn, window);
    xcb_get_geometry_reply_t* geomReply = xcb_get_geometry_reply(xcbConn, geomCookie, NULL);

    // then proceed to get an image

    if (!geomReply) return QPixmap();

    xcb_image_t* xcbImage = xcb_image_get(
                                xcbConn,
                                window,
                                geomReply->x,
                                geomReply->y,
                                geomReply->width,
                                geomReply->height,
                                ~0,
                                XCB_IMAGE_FORMAT_Z_PIXMAP
                                );


    // if the image is null, this means we need to get the root image window
    // and run a crop

    if (!xcbImage) {
        free(geomReply);
        return getWindowPixmap(QX11Info::appRootWindow(), blendPointer)
                .copy(geomReply->x, geomReply->y, geomReply->width, geomReply->height);
    }

    // now process the image

    QPixmap nativePixmap = convertFromNative(xcbImage);
    if (!(blendPointer)) {
        free(geomReply);
        free(xcbImage);
        return nativePixmap;
    }

    // now we blend in a pointer image

    xcb_get_geometry_cookie_t geomRootCookie = xcb_get_geometry_unchecked(xcbConn, geomReply->root);
    xcb_get_geometry_reply_t* geomRootReply = xcb_get_geometry_reply(xcbConn, geomRootCookie, NULL);

    xcb_translate_coordinates_cookie_t translateCookie = xcb_translate_coordinates_unchecked(
        xcbConn, window, geomReply->root, geomRootReply->x, geomRootReply->y);
    xcb_translate_coordinates_reply_t* translateReply = xcb_translate_coordinates_reply(
                                                            xcbConn, translateCookie, NULL);

    free(geomRootReply);
    free(translateReply);
    free(geomReply);
    free(xcbImage);

    return blendCursorImage(nativePixmap, translateReply->dst_x,translateReply->dst_y,
                            geomReply->width, geomReply->height);
}

QPixmap blendCursorImage(const QPixmap &pixmap, int x, int y, int width, int height)
{
    // first we get the cursor position, compute the co-ordinates of the region
    // of the screen we're grabbing, and see if the cursor is actually visible in
    // the region

    QPoint cursorPos = QCursor::pos();
    QRect screenRect(x, y, width, height);

    if (!(screenRect.contains(cursorPos))) {
        return pixmap;
    }

    // now we can get the image and start processing

    xcb_connection_t *xcbConn = QX11Info::connection();

    xcb_xfixes_get_cursor_image_cookie_t  cursorCookie = xcb_xfixes_get_cursor_image_unchecked(xcbConn);
    xcb_xfixes_get_cursor_image_reply_t* cursorReply = xcb_xfixes_get_cursor_image_reply(
                                                           xcbConn, cursorCookie, NULL);
    if (!cursorReply) {
        return pixmap;
    }

    quint32 *pixelData = xcb_xfixes_get_cursor_image_cursor_image(cursorReply);
    if (!pixelData) {
        return pixmap;
    }

    // process the image into a QImage

    QImage cursorImage = QImage((quint8 *)pixelData, cursorReply->width, cursorReply->height,
                                QImage::Format_ARGB32_Premultiplied);

    // a small fix for the cursor position for fancier cursors

    cursorPos -= QPoint(cursorReply->xhot, cursorReply->yhot);

    // now we translate the cursor point to our screen rectangle

    cursorPos -= QPoint(x, y);

    // and do the painting

    QPixmap blendedPixmap = pixmap;
    QPainter painter(&blendedPixmap);
    painter.drawImage(cursorPos, cursorImage);

    // and done
    free(cursorReply);

    return blendedPixmap;
}

static
bool lessThan ( const QRect& r1, const QRect& r2 )
{
    return r1.width() * r1.height() < r2.width() * r2.height();
}

// Recursively iterates over the window w and its children, thereby building
// a tree of window descriptors. Windows in non-viewable state or with height
// or width smaller than minSize will be ignored.
static
void getWindowsRecursive( QVector<QRect> &windows, xcb_window_t w,
              int rx = 0, int ry = 0, int depth = 0 )
{
    xcb_connection_t* c = QX11Info::connection();

    xcb_get_window_attributes_cookie_t ac = xcb_get_window_attributes_unchecked(c, w);
    xcb_get_window_attributes_reply_t *atts = xcb_get_window_attributes_reply(c, ac, NULL);

    xcb_get_geometry_cookie_t gc = xcb_get_geometry_unchecked(c, w);
    xcb_get_geometry_reply_t *geom = xcb_get_geometry_reply(c, gc, NULL);

    if ( atts && geom &&
            atts->map_state == XCB_MAP_STATE_VIEWABLE &&
            geom->width >= minSize && geom->height >= minSize ) {
        int x = 0, y = 0;
        if ( depth ) {
            x = geom->x + rx;
            y = geom->y + ry;
        }

        QRect r( x, y, geom->width, geom->height );
        if (!windows.contains(r))
            windows.append(r);

        xcb_query_tree_cookie_t tc = xcb_query_tree_unchecked(c, w);
        xcb_query_tree_reply_t *tree = xcb_query_tree_reply(c, tc, NULL);

        if (tree) {
            xcb_window_t* child = xcb_query_tree_children(tree);
            for (unsigned int i=0;i<tree->children_len;i++)
                getWindowsRecursive(windows, child[i], x, y, depth +1);
            free(tree);
        }
    }
    if (atts != NULL) free(atts);
    if (geom != NULL) free(geom);

    if ( depth == 0 )
        qSort(windows.begin(), windows.end(), lessThan);
}

static
xcb_window_t findRealWindow( xcb_window_t w, int depth = 0 )
{
    static char wm_state_s[] = "WM_STATE";

    xcb_connection_t* c = QX11Info::connection();

    if( depth > 5 ) {
        return 0;
    }

    xcb_intern_atom_cookie_t ac = xcb_intern_atom(c, 0, strlen(wm_state_s), wm_state_s);
    xcb_intern_atom_reply_t* wm_state = xcb_intern_atom_reply(c, ac, NULL);

    if (!wm_state) {
        qWarning("Unable to allocate xcb atom");
        return 0;
    }

    xcb_get_property_cookie_t pc = xcb_get_property(c, 0, w, wm_state->atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 0 );
    xcb_get_property_reply_t* pr = xcb_get_property_reply(c, pc, NULL);

    if (pr && pr->type != XCB_NONE) {
        free(pr);
        return w;
    }
    free(pr);
    free(wm_state);

    xcb_window_t ret = XCB_NONE;

    xcb_query_tree_cookie_t tc = xcb_query_tree_unchecked(c, w);
    xcb_query_tree_reply_t *tree = xcb_query_tree_reply(c, tc, NULL);

    if (tree) {
        xcb_window_t* child = xcb_query_tree_children(tree);
        for (unsigned int i=0;i<tree->children_len;i++)
            ret = findRealWindow(child[i], depth +1 );
        free(tree);
    }

    return ret;
}

static
xcb_window_t windowUnderCursor( bool includeDecorations = true )
{
    xcb_connection_t* c = QX11Info::connection();

    xcb_window_t child = QX11Info::appRootWindow();

    xcb_query_pointer_cookie_t pc = xcb_query_pointer(c,QX11Info::appRootWindow());
    xcb_query_pointer_reply_t *pr = xcb_query_pointer_reply(c, pc, NULL);

    if (pr && pr->child!=XCB_NONE )
        child = pr->child;

    free(pr);

    if( !includeDecorations ) {
        xcb_window_t real_child = findRealWindow( child );

        if( real_child != XCB_NONE ) { // test just in case
            child = real_child;
        }
    }

    return child;
}

QPoint WindowGrabber::windowPosition;
bool WindowGrabber::blendPointer = false;

WindowGrabber::WindowGrabber()
: QDialog( 0, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint ),
  current( -1 ), yPos( -1 )
{
    setWindowModality( Qt::WindowModal );
    int x, y, w, h;

    xcb_window_t child = windowUnderCursor();
    QPixmap pm( getWindowPixmap(child, blendPointer) );
    getWindowsRecursive( windows, child );
    getWindowGeometry(child, x, y, w, h);

    QPalette p = palette();
    p.setBrush( backgroundRole(), QBrush( pm ) );
    setPalette( p );
    setFixedSize( pm.size() );
    setMouseTracking( true );
    setGeometry( x, y, w, h );
    current = windowIndex( mapFromGlobal(QCursor::pos()) );
}

WindowGrabber::~WindowGrabber()
{
}

QPixmap WindowGrabber::grabCurrent( bool includeDecorations )
{
    xcb_connection_t* c = QX11Info::connection();

    int x, y, w, h;

    xcb_window_t child = windowUnderCursor( includeDecorations );

    xcb_query_tree_cookie_t tc = xcb_query_tree_unchecked(c, child);
    xcb_query_tree_reply_t *tree = xcb_query_tree_reply(c, tc, NULL);

    if (getWindowGeometry(child, x, y, w, h)) {
        if (tree) {
            xcb_translate_coordinates_cookie_t tc = xcb_translate_coordinates(
                                                        c, tree->parent, QX11Info::appRootWindow(), x, y);
            xcb_translate_coordinates_reply_t * tr = xcb_translate_coordinates_reply(c, tc, NULL);

            if (tr) {
                x = tr->dst_x;
                y = tr->dst_y;
                free(tr);
            }
        }
        windowPosition = QPoint(x,y);
    }

    QPixmap pm( getWindowPixmap(child, blendPointer) );
    return pm;
}


void WindowGrabber::mousePressEvent( QMouseEvent *e )
{
    if ( e->button() == Qt::RightButton ) {
        yPos = e->globalY();
    } else {
        if ( current != -1 ) {
            windowPosition = e->globalPos() - e->pos() + windows[current].topLeft();
            emit windowGrabbed( palette().brush( backgroundRole() ).texture().copy( windows[ current ] ) );
        } else {
            windowPosition = QPoint(0,0);
            emit windowGrabbed( QPixmap() );
        }
        accept();
    }
}

void WindowGrabber::mouseReleaseEvent( QMouseEvent *e )
{
    if ( e->button() == Qt::RightButton ) {
        yPos = -1;
    }
}

static
const int minDistance = 10;

void WindowGrabber::mouseMoveEvent( QMouseEvent *e )
{
    if ( yPos == -1 ) {
        int w = windowIndex( e->pos() );
        if ( w != -1 && w != current ) {
            current = w;
            repaint();
        }
    } else {
        int y = e->globalY();
        if ( y > yPos + minDistance ) {
            decreaseScope( e->pos() );
            yPos = y;
        } else if ( y < yPos - minDistance ) {
            increaseScope( e->pos() );
            yPos = y;
        }
    }
}

void WindowGrabber::wheelEvent( QWheelEvent *e )
{
    if ( e->delta() > 0 ) {
        increaseScope( e->pos() );
    } else if ( e->delta() < 0 ) {
        decreaseScope( e->pos() );
    } else {
        e->ignore();
    }
}

// Increases the scope to the next-bigger window containing the mouse pointer.
// This method is activated by either rotating the mouse wheel forwards or by
// dragging the mouse forwards while keeping the right mouse button pressed.
void WindowGrabber::increaseScope( const QPoint &pos )
{
    for ( int i = current + 1; i < windows.size(); i++ ) {
        if ( windows[ i ].contains( pos ) ) {
            current = i;
            break;
        }
    }
    repaint();
}

// Decreases the scope to the next-smaller window containing the mouse pointer.
// This method is activated by either rotating the mouse wheel backwards or by
// dragging the mouse backwards while keeping the right mouse button pressed.
void WindowGrabber::decreaseScope( const QPoint &pos )
{
    for ( int i = current - 1; i >= 0; i-- ) {
    if ( windows[ i ].contains( pos ) ) {
        current = i;
        break;
    }
    }
    repaint();
}

// Searches and returns the index of the first (=smallest) window
// containing the mouse pointer.
int WindowGrabber::windowIndex( const QPoint &pos ) const
{
    for ( int i = 0; i < windows.size(); i++ ) {
        if ( windows[ i ].contains( pos ) ) {
            return i;
        }
    }
    return -1;
}

// Draws a border around the (child) window currently containing the pointer
void WindowGrabber::paintEvent( QPaintEvent * )
{
    if ( current >= 0 ) {
        QPainter p;
        p.begin( this );
        p.fillRect(rect(), palette().brush( backgroundRole()));
        p.setPen( QPen( Qt::red, 3 ) );
        p.drawRect( windows[ current ].adjusted( 0, 0, -1, -1 ) );
        p.end();
    }
}
