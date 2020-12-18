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

#include <QBitmap>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGuiApplication>

#include "windowgrabber.h"
#include "xcbtools.h"

WindowGrabber::WindowGrabber(QWidget *parent, bool includeDecorations, bool blendPointer)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint)
{
    setWindowModality(Qt::WindowModal);
    QRect geom;

    xcb_window_t child = ZXCBTools::windowUnderCursor(includeDecorations);
    QPixmap pm(ZXCBTools::getWindowPixmap(child, blendPointer));
    ZXCBTools::getWindowsRecursive(windows, child);
    geom = ZXCBTools::getWindowGeometry(child);

    QPalette p = palette();
    p.setBrush(backgroundRole(), QBrush(pm));
    setPalette(p);
    setFixedSize(pm.size());
    setMouseTracking(true);
    if (!geom.isNull())
        setGeometry(geom);
    current = windowIndex(mapFromGlobal(QCursor::pos()));
}

WindowGrabber::~WindowGrabber() = default;

void WindowGrabber::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        yPos = event->globalPosition().toPoint().y();
#else
        yPos = event->globalY();
#endif
    } else {
        if (current != -1) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QRect windowRegion(event->globalPosition().toPoint() - event->pos() + windows.at(current).topLeft(),
                               windows.at(current).size());
#else
            QRect windowRegion(event->globalPos() - event->pos() + windows.at(current).topLeft(),
                               windows.at(current).size());
#endif
            Q_EMIT windowGrabbed(palette().brush(backgroundRole()).texture().copy(windows.at(current)),
                                 windowRegion);
        } else {
            Q_EMIT windowGrabbed(QPixmap(),QRect());
        }
        accept();
    }
}

void WindowGrabber::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
        yPos = -1;
}

void WindowGrabber::mouseMoveEvent(QMouseEvent *event)
{
    static const int minDistance = 10;

    if (yPos == -1) {
        int w = windowIndex(event->pos());
        if (w != -1 && w != current) {
            current = w;
            repaint();
        }
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        int y = event->globalPosition().toPoint().y();
#else
        int y = event->globalY();
#endif
        if (y > yPos + minDistance) {
            decreaseScope(event->pos());
            yPos = y;
        } else if (y < yPos - minDistance) {
            increaseScope(event->pos());
            yPos = y;
        }
    }
}

void WindowGrabber::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta > 0) {
        increaseScope(event->position().toPoint());
    } else if (delta < 0) {
        decreaseScope(event->position().toPoint());
    } else {
        event->ignore();
    }
}

// Increases the scope to the next-bigger window containing the mouse pointer.
// This method is activated by either rotating the mouse wheel forwards or by
// dragging the mouse forwards while keeping the right mouse button pressed.
void WindowGrabber::increaseScope(const QPoint &pos)
{
    for (int i = current + 1; i < windows.size(); i++) {
        if (windows.at(i).contains(pos)) {
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
    for (int i = current - 1; i >= 0; i--) {
        if (windows.at(i).contains(pos)) {
            current = i;
            break;
        }
    }
    repaint();
}

// Searches and returns the index of the first (=smallest) window
// containing the mouse pointer.
int WindowGrabber::windowIndex(const QPoint &pos) const
{
    for (int i = 0; i < windows.size(); i++) {
        if (windows[i].contains( pos ))
            return i;
    }
    return -1;
}

// Draws a border around the (child) window currently containing the pointer
void WindowGrabber::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (current >= 0) {
        QPainter p;
        p.begin(this);
        p.fillRect(rect(), palette().brush(backgroundRole()));
        p.setPen(QPen(Qt::red, 3));
        p.drawRect(windows.at(current).adjusted(0, 0, -1, -1));
        p.end();
    }
}
