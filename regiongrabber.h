/*
 *   Copyright (C) 2007 Luca Gugelmann <lucag@student.ethz.ch>
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef REGIONGRABBER_H
#define REGIONGRABBER_H

#include <QWidget>
#include <QRegion>
#include <QPoint>
#include <QVector>
#include <QRect>

class QPaintEvent;
class QResizeEvent;
class QMouseEvent;

class RegionGrabber : public QWidget
{
    Q_OBJECT

private:
    enum MaskType { StrokeMask, FillMask };

    QRect selection;
    QRect* mouseOverHandle { nullptr };
    QPoint dragStartPoint;
    QRect  selectionBeforeDrag;
    bool showHelp { true };
    bool grabbing { false };
    bool mouseDown { false };
    bool newSelection { false };
    const int handleSize { 10 };

    // naming convention for handles
    // T top, B bottom, R Right, L left
    // 2 letters: a corner
    // 1 letter: the handle on the middle of the corresponding side
    QRect TLHandle, TRHandle, BLHandle, BRHandle;
    QRect LHandle, THandle, RHandle, BHandle;
    QRect helpTextRect;

    QVector<QRect*> handles;
    QPixmap pixmap;

    void init(bool includePointer);
    void updateHandles();
    QRegion handleMask(MaskType typevent) const;
    QPoint limitPointToRect(const QPoint &p, const QRect &r) const;
    QRect normalizeSelection(const QRect &s) const;
    void grabRect();

public:
    RegionGrabber(QWidget *parent, const QRect &startSelection, bool includePointer);
    ~RegionGrabber() override;

Q_SIGNALS:
    void regionGrabbed(const QPixmap &pixmap, const QRect &region);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

};

#endif
