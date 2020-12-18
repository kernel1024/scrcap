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

#ifndef WINDOWGRABBER_H
#define WINDOWGRABBER_H

#include <QDialog>
#include <QVector>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

class WindowGrabber : public QDialog
{
    Q_OBJECT

private:
    QVector<QRect> windows;
    int current { -1 };
    int yPos { -1 };

    void increaseScope(const QPoint & pos);
    void decreaseScope(const QPoint & pos);
    int windowIndex(const QPoint & pos) const;

public:
    WindowGrabber(QWidget *parent, bool includeDecorations, bool blendPointer);
    ~WindowGrabber() override;

protected:
    void mousePressEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;
    void paintEvent(QPaintEvent * event) override;

Q_SIGNALS:
    void windowGrabbed(const QPixmap &pixmap, const QRect& windowRegion);
};

#endif // WINDOWGRABBER_H
