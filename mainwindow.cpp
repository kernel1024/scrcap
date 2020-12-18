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
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QImage>
#include <QPainter>
#include <QScreen>
#include <QWindow>
#include <QMessageBox>
#include <QClipboard>
#include <QThread>
#include <QMutexLocker>
#include <QDebug>

#include "mainwindow.h"
#include "funcs.h"
#include "windowgrabber.h"
#include "regiongrabber.h"
#include "xcbtools.h"
#include "qxtglobalshortcut.h"
#include "ui_mainwindow.h"

namespace CDefaults {
const MainWindow::ZCaptureMode captureMode = MainWindow::ZCaptureMode::FullScreen;
const int autocaptureDelay = 1000;
const int imageQuality = 90;
const int captureErrorTimerMS = 1000;
const int interactiveCaptureTimerMS = 200;
const bool includeDeco = true;
const bool includePointer = false;
const bool autocaptureWait = true;
const bool minimizeWindow = false;
const QSize previewSize(500,300);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->listMode->addItems(ZGenericFuncs::zCaptureMode());

    ui->listImgFormat->addItems(ZGenericFuncs::zImageFormats());
    ui->listImgFormat->setCurrentIndex(0);

    ui->btnSndPlay->setEnabled(beepPlayer.isGSTSupported());
    if (!ui->btnSndPlay->isEnabled())
        ui->btnSndPlay->setToolTip(tr("GStreamer support disabled."));

    autocaptureTimer.setSingleShot(false);

    lastRegion = QRect();
    savedAutocapImage = QImage();

    loadSettings();
    centerWindow();

    connect(ui->btnCapture, &QPushButton::clicked, this, &MainWindow::actionCapture);
    connect(ui->btnSave, &QPushButton::clicked, this, &MainWindow::saveAs);
    connect(ui->btnCopy, &QPushButton::clicked, this, &MainWindow::copyToClipboard);
    connect(ui->btnAutocapture, &QPushButton::toggled, this, &MainWindow::actionAutoCapture);
    connect(ui->btnDir, &QPushButton::clicked, this, &MainWindow::saveDirSelect);
    connect(ui->btnAutoSnd, &QPushButton::clicked, this, &MainWindow::autocaptureSndSelect);
    connect(ui->btnSndPlay, &QPushButton::clicked, this, &MainWindow::playSample);
    connect(ui->btnClearLog, &QPushButton::clicked, this, &MainWindow::clearLog);

    connect(ui->keyInteractive, &QKeySequenceEdit::editingFinished, this, &MainWindow::rebindHotkeys);
    connect(ui->keySilent, &QKeySequenceEdit::editingFinished, this, &MainWindow::rebindHotkeys);

    connect(&autocaptureTimer, &QTimer::timeout, this, &MainWindow::autoCapture);
    connect(&beepPlayer, &ZGSTPlayer::errorOccured, this, &MainWindow::addLogMessage);

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
    QScreen *screen = nullptr;

    if (window() && window()->windowHandle()) {
        screen = window()->windowHandle()->screen();
    } else if (!QApplication::screens().isEmpty()) {
        screen = QApplication::screenAt(QCursor::pos());
    }
    if (screen == nullptr)
        screen = QApplication::primaryScreen();

    QRect rect(screen->availableGeometry());
    move(rect.width()/2 - frameGeometry().width()/2,
         rect.height()/2 - frameGeometry().height()/2);
}

void MainWindow::loadSettings()
{
    QSettings settings;
    settings.beginGroup(QSL("global"));
    ui->listMode->setCurrentIndex(settings.value(QSL("mode"),CDefaults::captureMode).toInt());
    ui->spinDelay->setValue(settings.value(QSL("delay"),0).toInt());
    ui->spinAutocapInterval->setValue(settings.value(QSL("autocapDelay"),CDefaults::autocaptureDelay).toInt());

    QString s = settings.value(QSL("keyInteractive"),QString()).toString();
    if (!s.isEmpty()) {
        ui->keyInteractive->setKeySequence(QKeySequence::fromString(s));
    } else {
        ui->keyInteractive->clear();
    }
    s = settings.value(QSL("keySilent"),QString()).toString();
    if (!s.isEmpty()) {
        ui->keySilent->setKeySequence(QKeySequence::fromString(s));
    } else {
        ui->keySilent->clear();
    }

    ui->checkIncludeDeco->setChecked(settings.value(QSL("includeDeco"),CDefaults::includeDeco).toBool());
    ui->checkIncludePointer->setChecked(settings.value(QSL("includePointer"),CDefaults::includePointer).toBool());
    ui->checkAutocaptureWait->setChecked(settings.value(QSL("autocaptureWait"),CDefaults::autocaptureWait).toBool());
    ui->checkMinimize->setChecked(settings.value(QSL("minimizeWindow"),CDefaults::minimizeWindow).toBool());

    s = settings.value(QSL("imageFormat"),ZGenericFuncs::zImageFormats().first()).toString();
    int idx = ZGenericFuncs::zImageFormats().indexOf(s);
    if (idx>=0) {
        ui->listImgFormat->setCurrentIndex(idx);
    } else {
        ui->listImgFormat->setCurrentIndex(0);
    }
    ui->spinImgQuality->setValue(settings.value(QSL("imageQuality"),CDefaults::imageQuality).toInt());

    ui->editDir->setText(settings.value(QSL("saveDir"),
                                        QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
                         .toString());
    ui->editTemplate->setText(settings.value(QSL("filenameTemplate"),QSL("%NN")).toString());
    ui->editAutoSnd->setText(settings.value(QSL("autocaptureSound"),QString()).toString());
    settings.endGroup();

    rebindHotkeys();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.beginGroup(QSL("global"));
    settings.setValue(QSL("mode"),capMode());
    settings.setValue(QSL("delay"),ui->spinDelay->value());
    settings.setValue(QSL("autocapDelay"),ui->spinAutocapInterval->value());

    settings.setValue(QSL("keyInteractive"),ui->keyInteractive->keySequence().toString());
    settings.setValue(QSL("keySilent"),ui->keySilent->keySequence().toString());

    settings.setValue(QSL("includeDeco"),ui->checkIncludeDeco->isChecked());
    settings.setValue(QSL("includePointer"),ui->checkIncludePointer->isChecked());
    settings.setValue(QSL("autocaptureWait"),ui->checkAutocaptureWait->isChecked());
    settings.setValue(QSL("minimizeWindow"),ui->checkMinimize->isChecked());

    settings.setValue(QSL("imageFormat"),ui->listImgFormat->currentText());
    settings.setValue(QSL("imageQuality"),ui->spinImgQuality->value());

    settings.setValue(QSL("saveDir"),ui->editDir->text());
    settings.setValue(QSL("filenameTemplate"),ui->editTemplate->text());
    settings.setValue(QSL("autocaptureSound"),ui->editAutoSnd->text());
    settings.endGroup();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!saved) {
        int ret = QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
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
    QPixmap px = snapshot.scaled(CDefaults::previewSize,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    ui->imageDisplay->setPixmap(px);

    QString title = tr("%1 - screen capture").arg(QGuiApplication::applicationDisplayName());
    if (!saved)
        title.append(tr(" - unsaved"));
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
    const int oneK = 1000;

    hideWindow();

    int captureDelayMS = ui->spinDelay->value() * oneK;
    if (captureDelayMS < CDefaults::interactiveCaptureTimerMS)
        captureDelayMS = CDefaults::interactiveCaptureTimerMS;

    QTimer::singleShot(captureDelayMS,this,&MainWindow::interactiveCapture);
}

void MainWindow::actionAutoCapture(bool state)
{
    if (state) {
        if (lastRegion.isEmpty()) {
            QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                                 tr("Unable to start autocapture.\n"
                                    "You must make interactive snapshot first "
                                    "to mark out area for silent/automatic snapshots."));
            return;
        }

        hideWindow();
        autocaptureTimer.start(ui->spinAutocapInterval->value());
    } else {
        if (autocaptureTimer.isActive())
            autocaptureTimer.stop();
    }
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

    const QString fname = ZGenericFuncs::generateUniqName(ui->editTemplate->text(),
                                                          snapshot,
                                                          ui->editDir->text(),
                                                          ui->listImgFormat->currentText().toLower(),
                                                          false);
    QFileInfo fi(fname);
    if (!saveSnapshot(fname)) {
        QMessageBox::critical(nullptr,QGuiApplication::applicationDisplayName(),
                              tr("Unable to save file %1.").arg(fname));
    } else {
        ZGenericFuncs::sendDENotification(this,tr("Screenshot saved - %1").arg(fi.fileName()),
                                          QGuiApplication::applicationDisplayName(),ui->spinAutocapInterval->value());
    }
}

void MainWindow::autoCapture()
{
    QMutexLocker locker(&autoCaptureMutex);

    if (!lastRegion.isEmpty()) {
        QImage img = ZXCBTools::getWindowPixmap(ZXCBTools::appRootWindow(), false).copy(lastRegion).toImage();
        if (img.isNull()) {
            ui->btnAutocapture->setChecked(false);
            QTimer::singleShot(CDefaults::captureErrorTimerMS,this,[this](){
                QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
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
                const QString fname = ZGenericFuncs::generateUniqName(ui->editTemplate->text(),
                                                                      snapshot,
                                                                      ui->editDir->text(),
                                                                      ui->listImgFormat->currentText().toLower(),
                                                                      false);
                if (!saveSnapshot(fname)) {
                    ui->btnAutocapture->setChecked(false);
                    QTimer::singleShot(CDefaults::captureErrorTimerMS,this,[this,fname](){
                        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                                              tr("Unable to save file %1.").arg(fname));
                    });
                    return;
                }

                playSound(ui->editAutoSnd->text());
            }
        }
    } else {
        ui->btnAutocapture->setChecked(false);
        QTimer::singleShot(CDefaults::captureErrorTimerMS,this,[this](){
            QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                                  tr("Unable to start autocapture.\n"
                                     "You must make interactive snapshot first "
                                     "to mark out area for silent/automatic snapshots."));
        });
    }
}

void MainWindow::doCapture(const ZCaptureReason reason)
{
    int mode = capMode();
    bool includePointer = ui->checkIncludePointer->isChecked();
    bool includeDecorations = ui->checkIncludeDeco->isChecked();

    if (reason==SilentHotkey || reason==Autocapture) {
        if (!lastRegion.isEmpty()) {
            snapshot = ZXCBTools::getWindowPixmap(ZXCBTools::appRootWindow(), includePointer).copy(lastRegion);
            if (snapshot.isNull() && (reason!=Autocapture)) {
                QMessageBox::critical(nullptr,QGuiApplication::applicationDisplayName(),
                                      tr("Unable to make silent capture. XCB error, null snapshot received"));
            }
            updatePreview();
        } else {
            show();
            if (reason!=Autocapture) {
                QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),
                                     tr("Unable to make silent capture.\n"
                                        "You must make interactive snapshot first "
                                        "to mark out area for silent/automatic snapshots."));
            }
        }
        return;
    }

    if (reason==PreInit && mode==ChildWindow)
        mode = WindowUnderCursor;
    if (reason==PreInit && mode==Region)
        mode = WindowUnderCursor;

    bool interactive = false;

    if (mode==WindowUnderCursor) {

        snapshot = ZXCBTools::grabCurrent(includeDecorations, includePointer, &lastRegion);

        if (reason==UserSingle)
            saved = false;

    } else if (mode==ChildWindow) {

        WindowGrabber wndGrab(nullptr,includeDecorations,includePointer);
        connect(&wndGrab, &WindowGrabber::windowGrabbed,
                this, &MainWindow::windowGrabbed);
        wndGrab.exec();
        interactive = true;

    } else if (mode==Region) {

        auto* rgnGrab = new RegionGrabber(nullptr,lastGrabbedRegion,includePointer);
        connect(rgnGrab, &RegionGrabber::regionGrabbed,
                this, &MainWindow::regionGrabbed);
        interactive = true;

    } else if (mode==CurrentScreen) {

        QScreen *screen = nullptr;
        if (!QApplication::screens().isEmpty())
            screen = QApplication::screenAt(QCursor::pos());
        if (screen == nullptr)
            screen = QApplication::primaryScreen();

        QRect geom = screen->availableGeometry();
        snapshot = ZXCBTools::getWindowPixmap(ZXCBTools::appRootWindow(), includePointer).copy(geom);
        lastRegion = geom;

        if (reason==UserSingle)
            saved = false;

    } else if (mode==FullScreen) {

        snapshot = ZXCBTools::getWindowPixmap(ZXCBTools::appRootWindow(), includePointer);
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
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Unable to save file %1").arg(filename));
        return false;
    }
    saved = true;
    updatePreview();
    return true;
}

void MainWindow::playSound(const QString &filename)
{
    QUrl uri = QUrl::fromLocalFile(filename);
    if (!uri.isValid()) return;

    if (beepPlayer.media() != uri)
        beepPlayer.setMedia(uri);

    beepPlayer.play();
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
        saveDialogFilter = ZGenericFuncs::generateFilter(QStringList() << ui->listImgFormat->currentText());
    const QString uniq = ZGenericFuncs::generateUniqName(ui->editTemplate->text(),
                                                         snapshot,
                                                         ui->editDir->text());
    const QString fname = ZGenericFuncs::getSaveFileNameD(this,tr("Save screenshot"),ui->editDir->text(),
                                                          ZGenericFuncs::generateFilter(ZGenericFuncs::zImageFormats()),
                                                          &saveDialogFilter,
                                                          QFileDialog::DontUseNativeDialog |
                                                          QFileDialog::DontUseCustomDirectoryIcons,
                                                          uniq);

    if (fname.isNull() || fname.isEmpty()) return false;

    if (!saveSnapshot(fname)) {
        QMessageBox::warning(this,QGuiApplication::applicationDisplayName(),tr("File not saved."));
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
    const QString s = ZGenericFuncs::getExistingDirectoryD(this,tr("Directory for screenshots"),ui->editDir->text());
    if (!s.isEmpty())
        ui->editDir->setText(s);
}

void MainWindow::autocaptureSndSelect()
{
    const QString fname = ZGenericFuncs::getOpenFileNameD(this,tr("Sound file for autocapture event"),QString(),
                                                          tr("Audio files (*.mp3 *.wav *.ogg *.flac)"));
    if (!fname.isEmpty())
        ui->editAutoSnd->setText(fname);
}

void MainWindow::copyToClipboard()
{
    if (snapshot.isNull()) return;
    QApplication::clipboard()->setPixmap(snapshot);
}

void MainWindow::windowGrabbed(const QPixmap &pic, const QRect &region)
{
    snapshot = pic;
    saved = false;
    updatePreview();

    lastRegion = region;

    QApplication::restoreOverrideCursor();
    restoreWindow();
}

void MainWindow::regionGrabbed(const QPixmap &pic, const QRect& region)
{
    if (!pic.isNull()) {
        snapshot = pic;
        saved = false;
        updatePreview();
    }

    lastGrabbedRegion = region;
    lastRegion = region;

    auto* rgnGrab = qobject_cast<RegionGrabber *>(sender());
    if( capMode() == Region && rgnGrab)
        rgnGrab->deleteLater();


    QApplication::restoreOverrideCursor();
    restoreWindow();
}

void MainWindow::rebindHotkeys()
{
    if (!ui->keyInteractive->keySequence().isEmpty()) {
        if (keyInteractive)
            keyInteractive->deleteLater();
        keyInteractive = new QxtGlobalShortcut(ui->keyInteractive->keySequence(),this);
        connect(keyInteractive, &QxtGlobalShortcut::activated, this, &MainWindow::hotkeyInteractive);
        keyInteractive->setEnabled();
    } else {
        if (keyInteractive)
            keyInteractive->setDisabled();
    }

    if (!ui->keySilent->keySequence().isEmpty()) {
        if (keySilent)
            keySilent->deleteLater();
        keySilent = new QxtGlobalShortcut(ui->keySilent->keySequence(),this);
        connect(keySilent, &QxtGlobalShortcut::activated, this, &MainWindow::silentCaptureAndSave);
        keySilent->setEnabled();
    } else {
        if (keySilent)
            keySilent->setDisabled();
    }
}

void MainWindow::restoreWindow()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::addLogMessage(const QString &message)
{
    ui->editLog->moveCursor(QTextCursor::End);
    ui->editLog->insertPlainText(QSL("%1 %2\n").arg(QTime::currentTime().toString(QSL("HH:mm:ss")),message));
    ui->editLog->moveCursor(QTextCursor::End);
}

void MainWindow::clearLog()
{
    ui->editLog->clear();

}
