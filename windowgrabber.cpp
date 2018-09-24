/*
  From KSnapshot (KDE4). Straight ported to XCB by kernel1024.

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
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGuiApplication>
#include <QX11Info>

#include "xcbtools.h"

QPoint WindowGrabber::windowPosition;
QSize WindowGrabber::windowSize;
bool WindowGrabber::blendPointer = false;
bool WindowGrabber::includeDecorations = true;

WindowGrabber::WindowGrabber()
: QDialog( 0, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint ),
  current( -1 ), yPos( -1 )
{
    setWindowModality( Qt::WindowModal );
    int x, y, w, h;

    xcb_window_t child = windowUnderCursor( includeDecorations );
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

QPixmap WindowGrabber::grabCurrent( bool includeDecorations, bool includePointer )
{
    xcb_connection_t* c = QX11Info::connection();

    int x, y, w, h;

    xcb_window_t child = windowUnderCursor( includeDecorations );

    xcb_query_tree_cookie_t tc = xcb_query_tree_unchecked(c, child);
    xcb_query_tree_reply_t *tree = xcb_query_tree_reply(c, tc, nullptr);

    if (getWindowGeometry(child, x, y, w, h)) {
        if (tree) {
            xcb_translate_coordinates_cookie_t tc = xcb_translate_coordinates(
                                                        c, tree->parent, QX11Info::appRootWindow(), x, y);
            xcb_translate_coordinates_reply_t * tr = xcb_translate_coordinates_reply(c, tc, nullptr);

            if (tr) {
                x = tr->dst_x;
                y = tr->dst_y;
                free(tr);
            }
        }
        windowPosition = QPoint(x,y);
        windowSize = QSize(w, h);
    }

    QPixmap pm( getWindowPixmap(child, includePointer) );
    return pm;
}


void WindowGrabber::mousePressEvent( QMouseEvent *e )
{
    if ( e->button() == Qt::RightButton ) {
        yPos = e->globalY();
    } else {
        if ( current != -1 ) {
            windowPosition = e->globalPos() - e->pos() + windows[current].topLeft();
            windowSize = windows[current].size();
            emit windowGrabbed( palette().brush( backgroundRole() ).texture().copy( windows[ current ] ) );
        } else {
            windowPosition = QPoint(0,0);
            windowSize = QSize(0,0);
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
