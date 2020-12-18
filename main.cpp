#include <QApplication>
#include <QPointer>
#include "funcs.h"
#include "mainwindow.h"

QPointer<MainWindow> mainWindow;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    qSetMessagePattern(QSL("%{if-debug}Debug%{endif}"
                           "%{if-info}Info%{endif}"
                           "%{if-warning}Warning%{endif}"
                           "%{if-critical}Error%{endif}"
                           "%{if-fatal}Fatal%{endif}"
                           ": %{message} (%{file}:%{line})"));
    qInstallMessageHandler(ZGenericFuncs::stdConsoleOutput);

    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);
    QCoreApplication::setOrganizationName(QSL("kernel1024"));
    QCoreApplication::setApplicationName(QSL("scrcap"));
    QGuiApplication::setApplicationDisplayName(QSL("ScrCap"));

    MainWindow w;
    mainWindow = &w;
    w.show();
    
    return a.exec();
}
