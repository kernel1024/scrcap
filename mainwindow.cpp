/*
 *  Portions from KSnapshot (KDE), KScreengenie (KF5)
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
#include "regiongrabber.h"
#include "xcbtools.h"
#include "qxtglobalshortcut.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    keyInteractive = new QxtGlobalShortcut(this);
    keySilent = new QxtGlobalShortcut(this);
    keyInteractive->setDisabled();
    keySilent->setDisabled();

    ui->listMode->addItems(zCaptureMode);

    captureTimer = new QTimer(this);
    captureTimer->setSingleShot(true);

    lastRegion = QRect();

    loadSettings();
    centerWindow();

    fileCounter = -1;
    generateFilename(); // also initialize tooltips

    connect(ui->btnCapture, &QPushButton::clicked, this, &MainWindow::actionCapture);
    connect(ui->btnSave, &QPushButton::clicked, this, &MainWindow::saveAs);
    connect(ui->btnCopy, &QPushButton::clicked, this, &MainWindow::copyToClipboard);
    connect(ui->keyInteractive, &QKeySequenceEdit::editingFinished, this, &MainWindow::rebindHotkeys);
    connect(ui->keySilent, &QKeySequenceEdit::editingFinished, this, &MainWindow::rebindHotkeys);

    connect(keyInteractive, &QxtGlobalShortcut::activated, this, &MainWindow::actionCapture);

    connect(captureTimer,&QTimer::timeout,this,&MainWindow::interactiveCapture);

    doCapture(PreInit);
}

MainWindow::~MainWindow()
{
    delete ui;
}

int MainWindow::capMode()
{
    return ui->listMode->currentIndex();
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
    ui->spinAutocapInterval->setValue(settings.value("autocapDelay",1000).toInt());

    QString s = settings.value("keyInteractive",QString()).toString();
    if (!s.isEmpty())
        ui->keyInteractive->setKeySequence(QKeySequence::fromString(s));
    else
        ui->keyInteractive->clear();
    s = settings.value("keySilent",QString()).toString();
    if (!s.isEmpty())
        ui->keySilent->setKeySequence(QKeySequence::fromString(s));
    else
        ui->keySilent->clear();

    ui->checkIncludeDeco->setChecked(settings.value("includeDeco",true).toBool());
    ui->checkIncludePointer->setChecked(settings.value("includePointer",false).toBool());

    ui->editDir->setText(settings.value("saveDir",
                                        QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
                         .toString());
    ui->editTemplate->setText(settings.value("filenameTemplate","%NN").toString());
    settings.endGroup();

    rebindHotkeys();
}

void MainWindow::saveSettings()
{
    QSettings settings("kernel1024","scrcap");
    settings.beginGroup("global");
    settings.setValue("mode",capMode());
    settings.setValue("delay",ui->spinDelay->value());
    settings.setValue("autocapDelay",ui->spinAutocapInterval->value());

    settings.setValue("keyInteractive",ui->keyInteractive->keySequence().toString());
    settings.setValue("keySilent",ui->keySilent->keySequence().toString());

    settings.setValue("includeDeco",ui->checkIncludeDeco->isChecked());
    settings.setValue("includePointer",ui->checkIncludePointer->isChecked());

    settings.setValue("saveDir",ui->editDir->text());
    settings.setValue("filenameTemplate",ui->editTemplate->text());
    settings.endGroup();
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
    keyInteractive->setDisabled();
    keySilent->setDisabled();
    event->accept();
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
        QTimer::singleShot(200,this,&MainWindow::interactiveCapture);
}

void MainWindow::interactiveCapture()
{
    doCapture(UserSingle);
}

void MainWindow::doCapture(const ZCaptureReason reason)
{
    int mode = capMode();
    bool blendPointer = ui->checkIncludeDeco->isChecked();

    if (reason==PreInit && mode==ChildWindow)
        mode = WindowUnderCursor;
    if (reason==PreInit && mode==Region)
        mode = WindowUnderCursor;

    int x = -1;
    int y = -1;
    bool interactive = false;

    if (mode==WindowUnderCursor) {

        snapshot = WindowGrabber::grabCurrent( blendPointer );

        QPoint offset = WindowGrabber::lastWindowPosition();
        x = offset.x();
        y = offset.y();
        lastRegion = QRect(offset, WindowGrabber::lastWindowSize());

    } else if (mode==ChildWindow) {

        WindowGrabber::blendPointer = blendPointer;
        WindowGrabber wndGrab;
        connect(&wndGrab, &WindowGrabber::windowGrabbed,
                this, &MainWindow::windowGrabbed);
        wndGrab.exec();
        QPoint offset = wndGrab.lastWindowPosition();
        x = offset.x();
        y = offset.y();
        interactive = true;

    } else if (mode==Region) {

        RegionGrabber* rgnGrab = new RegionGrabber(lastGrabbedRegion);
        connect(rgnGrab, &RegionGrabber::regionGrabbed,
                this, &MainWindow::regionGrabbed);
        connect(rgnGrab, &RegionGrabber::regionUpdated,
                this, &MainWindow::regionUpdated);
        interactive = true;

    } else if (mode==CurrentScreen) {

        QDesktopWidget *desktop = QApplication::desktop();
        int screenId = desktop->screenNumber( QCursor::pos() );
        QRect geom = desktop->screenGeometry( screenId );
        x = geom.x();
        y = geom.y();
        snapshot = getWindowPixmap(QX11Info::appRootWindow(), blendPointer).copy(
                       x, y, geom.width(), geom.height());
        lastRegion = geom;

    } else if (mode==FullScreen) {

        x = 0;
        y = 0;
        snapshot = getWindowPixmap(QX11Info::appRootWindow(), blendPointer);
        lastRegion = QRect(QPoint(0,0),snapshot.size());

    }

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

    lastRegion = QRect(WindowGrabber::lastWindowPosition(),WindowGrabber::lastWindowSize());

    QApplication::restoreOverrideCursor();
    show();
}

void MainWindow::regionGrabbed(const QPixmap &pic)
{
    if ( !pic.isNull() )
    {
        snapshot = pic;
        updatePreview();
    }

    RegionGrabber* rgnGrab = qobject_cast<RegionGrabber *>(sender());
    if( capMode() == Region && rgnGrab)
        rgnGrab->deleteLater();

    lastRegion = lastGrabbedRegion;

    QApplication::restoreOverrideCursor();
    show();
}

void MainWindow::regionUpdated(const QRect &region)
{
    lastGrabbedRegion = region;
}

void MainWindow::rebindHotkeys()
{
    if (!ui->keyInteractive->keySequence().isEmpty()) {
        keyInteractive->setShortcut(ui->keyInteractive->keySequence());
        keyInteractive->setEnabled();
    } else
        keyInteractive->setDisabled();

    if (!ui->keySilent->keySequence().isEmpty()) {
        keySilent->setShortcut(ui->keySilent->keySequence());
        keySilent->setEnabled();
    } else
        keySilent->setDisabled();
}
