/*
 *  Portions from KSnapshot (KDE)
 *
 *  Copyright (C) 1997-2008 Richard J. Moore <rich@kde.org>
 *  Copyright (C) 2000 Matthias Ettrich <ettrich@troll.no>
 *  Copyright (C) 2002 Aaron J. Seigo <aseigo@kde.org>
 *  Copyright (C) 2003 Nadeem Hasan <nhasan@kde.org>
 *  Copyright (C) 2004 Bernd Brandstetter <bbrand@freenet.de>
 *  Copyright (C) 2006 Urs Wolfer <uwolfer @ kde.org>
 *  Copyright (C) 2010 Martin Gräßlin <kde@martin-graesslin.com>
 *  Copyright (C) 2010, 2011 Pau Garcia i Quiles <pgquiles@elpauer.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include <QApplication>
#include <QDesktopWidget>
#include <QSettings>
#include <QStandardPaths>
#include <QImage>
#include <QPainter>
#include <QScreen>
#include <QMessageBox>
#include <QDateTime>
#include <QClipboard>
#include <QX11Info>
#include <QDebug>

#include "mainwindow.h"
#include "funcs.h"
#include "windowgrabber.h"
#include "ui_mainwindow.h"

#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_util.h>
#include <xcb/xfixes.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initDone = false;

    ui->listMode->addItems(zCaptureMode);

    captureTimer = new QTimer(this);
    captureTimer->setSingleShot(true);

    loadSettings();
    centerWindow();

    fileCounter = -1;
    generateFilename(); // also initialize tooltips

    connect(ui->btnCapture,&QPushButton::clicked,this,&MainWindow::actionCapture);
    connect(ui->btnSave,&QPushButton::clicked,this,&MainWindow::saveAs);
    connect(ui->btnCopy,&QPushButton::clicked,this,&MainWindow::copyToClipboard);

    connect(captureTimer,&QTimer::timeout,this,&MainWindow::doCapture);

    doCapture();

    initDone = true;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::centerWindow()
{
    int screen = 0;
    QWidget *w = window();
    QDesktopWidget *desktop = QApplication::desktop();
    if (w) {
        screen = desktop->screenNumber(w);
    } else if (desktop->isVirtualDesktop()) {
        screen = desktop->screenNumber(QCursor::pos());
    } else {
        screen = desktop->screenNumber(this);
    }

    QRect rect(desktop->availableGeometry(screen));
    move(rect.width()/2 - frameGeometry().width()/2,
         rect.height()/2 - frameGeometry().height()/2);
}

void MainWindow::loadSettings()
{
    QSettings settings("kernel1024","scrcap");
    settings.beginGroup("global");
    ui->listMode->setCurrentIndex(settings.value("mode",FullScreen).toInt());
    ui->spinDelay->setValue(settings.value("delay",0).toInt());
    ui->checkIncludeDeco->setChecked(settings.value("includeDeco",true).toBool());
    ui->editDir->setText(settings.value("saveDir",
                                        QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
                         .toString());
    ui->editTemplate->setText(settings.value("filenameTemplate","%NN").toString());
    ui->checkIncludePointer->setChecked(settings.value("includePointer",false).toBool());
    settings.endGroup();
}

void MainWindow::grabPointerImage(xcb_connection_t* xcbConn, int x, int y)
{
    QPoint cursorPos = QCursor::pos();

    xcb_generic_error_t* err;

    xcb_xfixes_get_cursor_image_cookie_t cursorCookie = xcb_xfixes_get_cursor_image(xcbConn);
    xcb_xfixes_get_cursor_image_reply_t* cursorReply = xcb_xfixes_get_cursor_image_reply(
                                                           xcbConn, cursorCookie, &err);
    if (err && err->error_code!=0)
        qDebug() << "XCB error:" << err->error_code;

    if (cursorReply==NULL) return;

    quint32 *pixelData = xcb_xfixes_get_cursor_image_cursor_image(cursorReply);
    if (!pixelData) return;

    // process the image into a QImage

    QImage cursorImage = QImage((quint8 *)pixelData, cursorReply->width, cursorReply->height,
                                QImage::Format_ARGB32_Premultiplied);

    // a small fix for the cursor position for fancier cursors

    cursorPos -= QPoint(cursorReply->xhot, cursorReply->yhot);

    // now we translate the cursor point to our screen rectangle

    cursorPos -= QPoint(x, y);

    // and do the painting

    QPainter painter(&snapshot);
    painter.drawImage(cursorPos, cursorImage);

    free(cursorReply);
}

QString MainWindow::generateFilename()
{
    if (fileCounter<0) {
        ui->editTemplate->setToolTip(tr(
                                         "%NN - counter\n"
                                         "%w - width\n"
                                         "%h - height\n"
                                         "%y - year\n"
                                         "%m - month\n"
                                         "%d - day\n"
                                         "%t - time"
                                         ));
        fileCounter = 0;
        return QString();
    }

    fileCounter++;

    QString fname = ui->editTemplate->text();
    if (fname.isEmpty())
        fname = "%NN";

    QRegExp exp("%\\S+");
    int pos = 0;
    while ((pos=exp.indexIn(fname,pos)) != -1) {
        int length = exp.matchedLength();
        if (length>=2) {
            QString tl = fname.mid(pos+1,length-1);
            if (tl.contains(QRegExp("N+")))
                fname.replace(pos, length, QString("%1").arg(fileCounter,tl.length(),10,QChar('0')));
            else if (tl == "w")
                fname.replace(pos, length, QString("%1").arg(snapshot.width()));
            else if (tl == "h")
                fname.replace(pos, length, QString("%1").arg(snapshot.height()));
            else if (tl == "y")
                fname.replace(pos, length, QDateTime::currentDateTime().toString("yyyy"));
            else if (tl == "m")
                fname.replace(pos, length, QDateTime::currentDateTime().toString("MM"));
            else if (tl == "d")
                fname.replace(pos, length, QDateTime::currentDateTime().toString("dd"));
            else if (tl == "t")
                fname.replace(pos, length, QDateTime::currentDateTime().toString("hh-mm-ss"));
        }
        pos += length;
    }
    return fname;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

void MainWindow::saveSettings()
{
    QSettings settings("kernel1024","scrcap");
    settings.beginGroup("global");
    settings.setValue("mode",ui->listMode->currentIndex());
    settings.setValue("delay",ui->spinDelay->value());
    settings.setValue("includeDeco",ui->checkIncludeDeco->isChecked());
    settings.setValue("saveDir",ui->editDir->text());
    settings.setValue("filenameTemplate",ui->editTemplate->text());
    settings.setValue("includePointer",ui->checkIncludePointer->isChecked());
    settings.endGroup();
}

void MainWindow::updatePreview()
{
    QPixmap px = snapshot.scaled(500,300,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    ui->imageDisplay->setPixmap(px);
}

void MainWindow::actionCapture()
{
    hide();
    QApplication::processEvents();
    if (ui->spinDelay->value()>0)
        captureTimer->start(ui->spinDelay->value()*1000);
    else
        QTimer::singleShot(200,this,&MainWindow::doCapture);
}

void MainWindow::doCapture()
{
    int mode = ui->listMode->currentIndex();

    if (!initDone && mode==ChildWindow)
        mode = WindowUnderCursor;

    int x = -1;
    int y = -1;
    bool interactive = false;

    if (mode==WindowUnderCursor) {
        snapshot = WindowGrabber::grabCurrent( ui->checkIncludeDeco->isChecked() );

        QPoint offset = WindowGrabber::lastWindowPosition();
        x = offset.x();
        y = offset.y();

    } else if (mode==ChildWindow) {
        WindowGrabber::blendPointer = ui->checkIncludePointer->isChecked();
        WindowGrabber wndGrab;
        connect(&wndGrab, &WindowGrabber::windowGrabbed,
                this, &MainWindow::windowGrabbed);
        wndGrab.exec();
        QPoint offset = wndGrab.lastWindowPosition();
        x = offset.x();
        y = offset.y();
        interactive = true;

    } else if (mode==Region) {
        // TODO: region capture

    } else if (mode==CurrentScreen) {
        QDesktopWidget *desktop = QApplication::desktop();
        int screenId = desktop->screenNumber( QCursor::pos() );
        QRect geom = desktop->screenGeometry( screenId );
        x = geom.x();
        y = geom.y();
        snapshot = QGuiApplication::primaryScreen()->grabWindow(desktop->winId(),
                x, y, geom.width(), geom.height() );

    } else if (mode==FullScreen) {
        x = 0;
        y = 0;
        snapshot = QGuiApplication::primaryScreen()->grabWindow( QApplication::desktop()->winId() );
    }

    if ( ui->checkIncludePointer->isChecked() && x>=0 && y>=0 )
        grabPointerImage(QX11Info::connection(), x, y);

    if (!interactive) {
        updatePreview();
        show();
    }
}

void MainWindow::saveAs()
{
    if (snapshot.isNull()) return;

    if (saveDialogFilter.isEmpty())
        saveDialogFilter = tr("PNG (*.png)");
    QString fname = getSaveFileNameD(this,tr("Save screenshot"),ui->editDir->text(),
                                     tr("JPEG (*.jpg);;PNG (*.png)"),&saveDialogFilter,
                                     0,generateFilename());

    if (fname.isNull() || fname.isEmpty()) return;

    if (!snapshot.save(fname))
        QMessageBox::critical(this,tr("ScrCap error"),tr("Unable to save file %1").arg(fname));
}

void MainWindow::saveDirSelect()
{
    QString s = getExistingDirectoryD(this,tr("Directory for screenshots"),ui->editDir->text());
    if (!s.isEmpty())
        ui->editDir->setText(s);
}

void MainWindow::copyToClipboard()
{
    if (snapshot.isNull()) return;
    QApplication::clipboard()->setPixmap(snapshot);
}

void MainWindow::windowGrabbed(const QPixmap &pic)
{
    snapshot = pic;
    updatePreview();
    QApplication::restoreOverrideCursor();
    show();
}
