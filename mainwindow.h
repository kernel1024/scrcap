#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QCloseEvent>
#include <QTimer>
#include <QPixmap>

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
        Hotkey=3
    };
    Q_ENUM(ZCaptureReason)

    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    int capMode();

private:
    Ui::MainWindow *ui;
    QTimer* captureTimer;
    QPixmap snapshot;
    bool haveXFixes;
    QString saveDialogFilter;
    int fileCounter;
    QRect lastGrabbedRegion;
    QRect lastRegion;

    QxtGlobalShortcut* keyInteractive;
    QxtGlobalShortcut* keySilent;

    void centerWindow();
    void loadSettings();
    QString generateFilename();
    void doCapture(const ZCaptureReason reason);


protected:
    void closeEvent(QCloseEvent *event);

public slots:
    void saveSettings();
    void updatePreview();
    void actionCapture();
    void interactiveCapture();
    void saveAs();
    void saveDirSelect();
    void copyToClipboard();
    void windowGrabbed(const QPixmap& pic);
    void regionGrabbed(const QPixmap& pic);
    void regionUpdated(const QRect& region);
    void rebindHotkeys();

};

#endif // MAINWINDOW_H
