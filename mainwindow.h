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
    bool initDone;
    QRect lastRegion;

    void centerWindow();
    void loadSettings();
    QString generateFilename();

protected:
    void closeEvent(QCloseEvent *event);

public slots:
    void saveSettings();
    void updatePreview();
    void actionCapture();
    void doCapture();
    void saveAs();
    void saveDirSelect();
    void copyToClipboard();
    void windowGrabbed(const QPixmap& pic);
    void regionGrabbed(const QPixmap& pic);
    void regionUpdated(const QRect& region);

};

#endif // MAINWINDOW_H
