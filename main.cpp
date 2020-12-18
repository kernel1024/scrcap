#include <QApplication>
#include "funcs.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);
    QCoreApplication::setOrganizationName(QSL("kernel1024"));
    QCoreApplication::setApplicationName(QSL("scrcap"));
    QGuiApplication::setApplicationDisplayName(QSL("ScrCap"));

    MainWindow w;
    w.show();
    
    return a.exec();
}
