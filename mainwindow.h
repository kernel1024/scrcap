#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QCloseEvent>
#include <QTimer>
#include <QPixmap>
#include <QMutex>
#include "funcs.h"
#include "gstplayer.h"

namespace Ui {
class MainWindow;
}

class QxtGlobalShortcut;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum ZCaptureMode {
        FullScreen=0,
        CurrentScreen=1,
        WindowUnderCursor=2,
        Region=3,
        ChildWindow=4
    };
    Q_ENUM(ZCaptureMode)

    enum ZCaptureReason {
        PreInit=0,
        UserSingle=1,
        Autocapture=2,
        SilentHotkey=3
    };
    Q_ENUM(ZCaptureReason)

    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow() override;
    int capMode();

private:
    Ui::MainWindow *ui;
    QPointer<QxtGlobalShortcut> keyInteractive;
    QPointer<QxtGlobalShortcut> keySilent;
    ZGSTPlayer beepPlayer;
    QMutex autoCaptureMutex;
    QImage savedAutocapImage;
    QTimer autocaptureTimer;
    QPixmap snapshot;
    QString saveDialogFilter;
    QRect lastGrabbedRegion;
    QRect lastRegion;
    bool saved { true };

    void centerWindow();
    void loadSettings();
    void doCapture(const ZCaptureReason reason);
    bool saveSnapshot(const QString& filename);
    void playSound(const QString& filename);

    void hideWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

public Q_SLOTS:
    void saveSettings();
    void updatePreview();
    void hotkeyInteractive();
    void actionCapture();
    void actionAutoCapture(bool state);
    void interactiveCapture();
    void silentCaptureAndSave();
    void autoCapture();
    bool saveAs();
    void playSample();
    void saveDirSelect();
    void autocaptureSndSelect();
    void copyToClipboard();
    void windowGrabbed(const QPixmap& pic, const QRect &region);
    void regionGrabbed(const QPixmap& pic, const QRect &region);
    void rebindHotkeys();
    void restoreWindow();
    void addLogMessage(const QString& message);
    void clearLog();

};

#endif // MAINWINDOW_H
