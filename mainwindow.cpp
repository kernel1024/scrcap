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
#include <QClipboard>
#include <QX11Info>
#include <QThread>
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

    saved = true;

    keyInteractive = new QxtGlobalShortcut(this);
    keySilent = new QxtGlobalShortcut(this);
    keyInteractive->setDisabled();
    keySilent->setDisabled();

    ui->listMode->addItems(zCaptureMode);

    ui->listImgFormat->addItems(zImageFormats);
    ui->listImgFormat->setCurrentIndex(0);

    captureTimer = new QTimer(this);
    captureTimer->setSingleShot(true);

    autocaptureTimer = new QTimer(this);
    autocaptureTimer->setSingleShot(false);

    lastRegion = QRect();
    savedAutocapImage = QImage();
    beepPlayer = new QMediaPlayer(this,QMediaPlayer::LowLatency);

    loadSettings();
    centerWindow();

    connect(ui->btnCapture, &QPushButton::clicked, this, &MainWindow::actionCapture);
    connect(ui->btnSave, &QPushButton::clicked, this, &MainWindow::saveAs);
    connect(ui->btnCopy, &QPushButton::clicked, this, &MainWindow::copyToClipboard);
    connect(ui->btnAutocapture, &QPushButton::toggled, this, &MainWindow::actionAutoCapture);
    connect(ui->btnDir, &QPushButton::clicked, this, &MainWindow::saveDirSelect);
    connect(ui->btnAutoSnd, &QPushButton::clicked, this, &MainWindow::autocaptureSndSelect);
    connect(ui->btnSndPlay, &QPushButton::clicked, this, &MainWindow::playSample);

    connect(ui->keyInteractive, &QKeySequenceEdit::editingFinished, this, &MainWindow::rebindHotkeys);
    connect(ui->keySilent, &QKeySequenceEdit::editingFinished, this, &MainWindow::rebindHotkeys);

    connect(keyInteractive, &QxtGlobalShortcut::activated, this, &MainWindow::hotkeyInteractive);
    connect(keySilent, &QxtGlobalShortcut::activated, this, &MainWindow::silentCaptureAndSave);

    connect(captureTimer, &QTimer::timeout, this, &MainWindow::interactiveCapture);
    connect(autocaptureTimer, &QTimer::timeout, this, &MainWindow::autoCapture);

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
    ui->checkAutocaptureWait->setChecked(settings.value("autocaptureWait",true).toBool());
    ui->checkMinimize->setChecked(settings.value("minimizeWindow",false).toBool());

    s = settings.value("imageFormat","PNG").toString();
    int idx = zImageFormats.indexOf(s);
    if (idx>=0)
        ui->listImgFormat->setCurrentIndex(idx);
    else
        ui->listImgFormat->setCurrentIndex(0);
    ui->spinImgQuality->setValue(settings.value("imageQuality",90).toInt());

    ui->editDir->setText(settings.value("saveDir",
                                        QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
                         .toString());
    ui->editTemplate->setText(settings.value("filenameTemplate","%NN").toString());
    ui->editAutoSnd->setText(settings.value("autocaptureSound",QString()).toString());
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
    settings.setValue("autocaptureWait",ui->checkAutocaptureWait->isChecked());
    settings.setValue("minimizeWindow",ui->checkMinimize->isChecked());

    settings.setValue("imageFormat",ui->listImgFormat->currentText());
    settings.setValue("imageQuality",ui->spinImgQuality->value());

    settings.setValue("saveDir",ui->editDir->text());
    settings.setValue("filenameTemplate",ui->editTemplate->text());
    settings.setValue("autocaptureSound",ui->editAutoSnd->text());
    settings.endGroup();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!saved) {
        int ret = QMessageBox::warning(this,tr("ScrCap"),
                                       tr("The screenshot has been modified. Do you want to save?"),
                                       QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                       QMessageBox::Save);
        if (ret == QMessageBox::Save) {
            if (!saveAs()) {
                event->ignore();
                return;
            }
        } else if (ret == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
    }

    saveSettings();
    keyInteractive->setDisabled();
    keySilent->setDisabled();
    event->accept();
}

void MainWindow::updatePreview()
{
    QPixmap px = snapshot.scaled(500,300,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    ui->imageDisplay->setPixmap(px);

    QString title = tr("ScrCap - screen capture");
    if (!saved)
        title += tr(" - unsaved");
    setWindowTitle(title);
}

void MainWindow::hotkeyInteractive()
{
    if (isVisible()) {
        actionCapture();
        return;
    }

    if (ui->btnAutocapture->isChecked())
        ui->btnAutocapture->setChecked(false);

    restoreWindow();
}

void MainWindow::actionCapture()
{
    hideWindow();
    if (ui->spinDelay->value()>0)
        captureTimer->start(ui->spinDelay->value()*1000);
    else
        QTimer::singleShot(200,this,&MainWindow::interactiveCapture);
}

void MainWindow::actionAutoCapture(bool state)
{
    if (state) {
        if (lastRegion.isEmpty()) {
            QMessageBox::warning(this,tr("ScrCap"),
                                 tr("Unable to start autocapture.\n"
                                    "You must make interactive snapshot first "
                                    "to mark out area for silent/automatic snapshots."));
            return;
        }

        hideWindow();
        autocaptureTimer->start(ui->spinAutocapInterval->value());
    } else
        if (autocaptureTimer->isActive())
            autocaptureTimer->stop();
}

void MainWindow::interactiveCapture()
{
    doCapture(UserSingle);
}

void MainWindow::silentCaptureAndSave()
{
    hideWindow();

    doCapture(SilentHotkey);

    if (snapshot.isNull()) return;

    QString fname = generateUniqName(ui->editTemplate->text(),
                                     snapshot,
                                     ui->editDir->text(),
                                     ui->listImgFormat->currentText().toLower(),
                                     false);
    QFileInfo fi(fname);
    if (!saveSnapshot(fname))
        QMessageBox::critical(nullptr,tr("ScrCap error"),tr("Unable to save file %1.").arg(fname));
    else
        sendDENotification(this,tr("Screenshot saved - %1").arg(fi.fileName()),
                           tr("ScrCap"),ui->spinAutocapInterval->value());
}

void MainWindow::autoCapture()
{
    autoCaptureMutex.lock();

    if (!lastRegion.isEmpty()) {
        QImage img = getWindowPixmap(QX11Info::appRootWindow(), false).copy(lastRegion).toImage();
        if (img.isNull()) {
            ui->btnAutocapture->setChecked(false);
            autoCaptureMutex.unlock();
            QTimer::singleShot(1000,[this](){
                QMessageBox::critical(this,tr("ScrCap error"),
                                      tr("Unable to make silent capture. XCB error, null snapshot received"));
            });
            return;
        }
        if (img!=savedAutocapImage) {
            savedAutocapImage = img;

            if (ui->checkAutocaptureWait->isChecked() && ui->spinAutocapInterval->value()>0)
                QThread::msleep(ui->spinAutocapInterval->value());

            doCapture(Autocapture);

            if (!snapshot.isNull()) {
                QString fname = generateUniqName(ui->editTemplate->text(),
                                                 snapshot,
                                                 ui->editDir->text(),
                                                 ui->listImgFormat->currentText().toLower(),
                                                 false);
                if (!saveSnapshot(fname)) {
                    ui->btnAutocapture->setChecked(false);
                    autoCaptureMutex.unlock();
                    QTimer::singleShot(1000,[this,fname](){
                        QMessageBox::critical(this,tr("ScrCap error"),tr("Unable to save file %1.").arg(fname));
                    });
                    return;
                } else
                    playSound(ui->editAutoSnd->text());
            }
        }
    } else {
        ui->btnAutocapture->setChecked(false);
        QTimer::singleShot(1000,[this](){
            QMessageBox::critical(this,tr("ScrCap error"),
                                  tr("Unable to start autocapture.\n"
                                     "You must make interactive snapshot first "
                                     "to mark out area for silent/automatic snapshots."));
        });
    }

    autoCaptureMutex.unlock();
}

void MainWindow::doCapture(const ZCaptureReason reason)
{
    int mode = capMode();
    bool blendPointer = ui->checkIncludeDeco->isChecked();

    if (reason==SilentHotkey || reason==Autocapture) {
        if (!lastRegion.isEmpty()) {
            snapshot = getWindowPixmap(QX11Info::appRootWindow(), blendPointer).copy(lastRegion);
            if (snapshot.isNull() && (reason!=Autocapture))
                QMessageBox::critical(nullptr,tr("ScrCap error"),
                                      tr("Unable to make silent capture. XCB error, null snapshot received"));
            updatePreview();
        } else {
            show();
            if (reason!=Autocapture)
                QMessageBox::warning(this,tr("ScrCap"),
                                     tr("Unable to make silent capture.\n"
                                        "You must make interactive snapshot first "
                                        "to mark out area for silent/automatic snapshots."));
        }
        return;
    }

    if (reason==PreInit && mode==ChildWindow)
        mode = WindowUnderCursor;
    if (reason==PreInit && mode==Region)
        mode = WindowUnderCursor;

    bool interactive = false;

    if (mode==WindowUnderCursor) {

        snapshot = WindowGrabber::grabCurrent( blendPointer );

        QPoint offset = WindowGrabber::lastWindowPosition();
        lastRegion = QRect(offset, WindowGrabber::lastWindowSize());

        if (reason==UserSingle)
            saved = false;

    } else if (mode==ChildWindow) {

        WindowGrabber::blendPointer = blendPointer;
        WindowGrabber wndGrab;
        connect(&wndGrab, &WindowGrabber::windowGrabbed,
                this, &MainWindow::windowGrabbed);
        wndGrab.exec();
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
        snapshot = getWindowPixmap(QX11Info::appRootWindow(), blendPointer).copy(geom);
        lastRegion = geom;

        if (reason==UserSingle)
            saved = false;

    } else if (mode==FullScreen) {

        snapshot = getWindowPixmap(QX11Info::appRootWindow(), blendPointer);
        lastRegion = QRect(QPoint(0,0),snapshot.size());

        if (reason==UserSingle)
            saved = false;

    }

    if (!interactive) {
        updatePreview();
        restoreWindow();
    }
}

bool MainWindow::saveSnapshot(const QString &filename)
{
    if (!snapshot.save(filename,nullptr,ui->spinImgQuality->value())) {
        QMessageBox::critical(this,tr("ScrCap error"),tr("Unable to save file %1").arg(filename));
        return false;
    }
    saved = true;
    updatePreview();
    return true;
}

void MainWindow::playSound(const QString &filename)
{
    QFileInfo fi(filename);
    QUrl fname = QUrl::fromLocalFile(filename);
    if (!fname.isValid() || !fi.exists()) return;

    if (beepPlayer->media().canonicalUrl()!=fname)
        beepPlayer->setMedia(fname);

    beepPlayer->play();
}

void MainWindow::hideWindow()
{
    if (ui->checkMinimize->isChecked()) {
        if (!isMinimized())
            showMinimized();
    } else {
        if (isVisible())
            hide();
    }
    QApplication::processEvents();
}

bool MainWindow::saveAs()
{
    if (snapshot.isNull()) return false;

    if (saveDialogFilter.isEmpty())
        saveDialogFilter = generateFilter(QStringList() << ui->listImgFormat->currentText());
    QString uniq = generateUniqName(ui->editTemplate->text(),
                                    snapshot,
                                    ui->editDir->text());
    QString fname = getSaveFileNameD(this,tr("Save screenshot"),ui->editDir->text(),
                                     generateFilter(zImageFormats),&saveDialogFilter,
                                     0,uniq);

    if (fname.isNull() || fname.isEmpty()) return false;

    if (!saveSnapshot(fname)) {
        QMessageBox::warning(this,tr("ScrCap"),tr("File not saved."));
        return false;
    }
    return true;
}

void MainWindow::playSample()
{
    playSound(ui->editAutoSnd->text());
}

void MainWindow::saveDirSelect()
{
    QString s = getExistingDirectoryD(this,tr("Directory for screenshots"),ui->editDir->text());
    if (!s.isEmpty())
        ui->editDir->setText(s);
}

void MainWindow::autocaptureSndSelect()
{
    QString fname = getOpenFileNameD(this,tr("Sound file for autocapture event"),QString(),
                                     tr("Audio files (*.mp3 *.wav *.ogg *.flac)"));
    if (!fname.isEmpty())
        ui->editAutoSnd->setText(fname);
}

void MainWindow::copyToClipboard()
{
    if (snapshot.isNull()) return;
    QApplication::clipboard()->setPixmap(snapshot);
}

void MainWindow::windowGrabbed(const QPixmap &pic)
{
    snapshot = pic;
    saved = false;
    updatePreview();

    lastRegion = QRect(WindowGrabber::lastWindowPosition(),WindowGrabber::lastWindowSize());

    QApplication::restoreOverrideCursor();
    restoreWindow();
}

void MainWindow::regionGrabbed(const QPixmap &pic)
{
    if ( !pic.isNull() )
    {
        snapshot = pic;
        saved = false;
        updatePreview();
    }

    RegionGrabber* rgnGrab = qobject_cast<RegionGrabber *>(sender());
    if( capMode() == Region && rgnGrab)
        rgnGrab->deleteLater();

    lastRegion = lastGrabbedRegion;

    QApplication::restoreOverrideCursor();
    restoreWindow();
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

void MainWindow::restoreWindow()
{
    showNormal();
    raise();
    activateWindow();
}
