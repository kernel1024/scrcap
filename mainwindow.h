#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QCloseEvent>
#include <QTimer>
#include <QPixmap>
#include <QMutex>
#include <QMediaPlayer>

namespace Ui {
class MainWindow;
}

static const QStringList zCaptureMode = {
    "Full screen",
    "Current screen",
    "Window under cursor",
    "Rectangle region",
    "Child window"
};

static const QStringList zImageFormats = {
    "PNG",
    "JPG"
};

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
    ~MainWindow();
    int capMode();

private:
    Ui::MainWindow *ui;
    QTimer* captureTimer;
    QTimer* autocaptureTimer;
    QPixmap snapshot;
    bool haveXFixes;
    QString saveDialogFilter;
    QRect lastGrabbedRegion;
    QRect lastRegion;
    bool saved;

    QMutex autoCaptureMutex;
    QImage savedAutocapImage;
    QMediaPlayer* beepPlayer;

    QxtGlobalShortcut* keyInteractive;
    QxtGlobalShortcut* keySilent;

    void centerWindow();
    void loadSettings();
    void doCapture(const ZCaptureReason reason);
    bool saveSnapshot(const QString& filename);
    void playSound(const QString& filename);

    void hideWindow();

protected:
    void closeEvent(QCloseEvent *event);

public slots:
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
    void windowGrabbed(const QPixmap& pic);
    void regionGrabbed(const QPixmap& pic);
    void regionUpdated(const QRect& region);
    void rebindHotkeys();
    void restoreWindow();

};

#endif // MAINWINDOW_H
