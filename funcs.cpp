#include <QDir>
#include <QDateTime>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QWidget>
#include <QIcon>
#include <QMutex>
#include <QRegularExpression>

extern "C" {
#include <unistd.h>
}

#include "funcs.h"
#include "mainwindow.h"

ZGenericFuncs::ZGenericFuncs(QObject *parent)
    : QObject(parent)
{
}

ZGenericFuncs::~ZGenericFuncs() = default;

void ZGenericFuncs::stdConsoleOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static QMutex loggerMutex;
    QMutexLocker locker(&loggerMutex);

    const QString fmsg = qFormatLogMessage(type,context,msg);
    if (fmsg.isEmpty()) return;

    fprintf(stderr, "%s\n", fmsg.toLocal8Bit().constData()); // NOLINT

    if (mainWindow) {
        QMetaObject::invokeMethod(mainWindow,[fmsg](){
            mainWindow->addLogMessage(fmsg);
        },Qt::QueuedConnection);
    }
}

const QStringList &ZGenericFuncs::zCaptureMode() {
    static const QStringList res = {
        QSL("Full screen"),
        QSL("Current screen"),
        QSL("Window under cursor"),
        QSL("Rectangle region"),
        QSL("Child window")
    };
    return res;
}

const QStringList &ZGenericFuncs::zImageFormats() {
    static const QStringList res = {
        QSL("PNG"),
        QSL("JPG")
    };
    return res;
}

QStringList ZGenericFuncs::getSuffixesFromFilter(const QString& filter)
{
    QStringList res;
    if (filter.isEmpty()) return res;

    res = filter.split(QSL(";;"),Qt::SkipEmptyParts);
    if (res.isEmpty()) return res;
    QString ex = res.first();
    res.clear();

    if (ex.isEmpty()) return res;

    ex.remove(QRegularExpression(QSL("^.*\\(")));
    ex.remove(QRegularExpression(QSL("\\).*$")));
    ex.remove(QRegularExpression(QSL("^.*\\.")));
    res = ex.split(QSL(" "));

    return res;
}

QString ZGenericFuncs::getOpenFileNameD(QWidget * parent, const QString & caption, const QString & dir,
                                        const QString & filter, QString * selectedFilter,
                                        QFileDialog::Options options)
{
    return QFileDialog::getOpenFileName(parent,caption,dir,filter,selectedFilter,options);
}

QString ZGenericFuncs::getSaveFileNameD(QWidget * parent, const QString & caption, const QString & dir,
                                        const QString & filter, QString * selectedFilter, QFileDialog::Options options,
                                        const QString & preselectFileName)
{
    QFileDialog d(parent,caption,dir,filter);
    d.setFileMode(QFileDialog::AnyFile);
    d.setOptions(options);
    d.setAcceptMode(QFileDialog::AcceptSave);

    QStringList sl;
    if (selectedFilter!=nullptr && !selectedFilter->isEmpty()) {
        sl=getSuffixesFromFilter(*selectedFilter);
    } else {
        sl=getSuffixesFromFilter(filter);
    }
    if (!sl.isEmpty())
        d.setDefaultSuffix(sl.first());

    if (selectedFilter && !selectedFilter->isEmpty())
        d.selectNameFilter(*selectedFilter);

    if (!preselectFileName.isEmpty())
        d.selectFile(preselectFileName);

    QString res;
    if (d.exec()==QDialog::Accepted) {
        QString userFilter = d.selectedNameFilter();
        if (selectedFilter)
            *selectedFilter=userFilter;
        if (!userFilter.isEmpty()) {
            const auto suffixes = getSuffixesFromFilter(userFilter);
            d.setDefaultSuffix(suffixes.first());
        }

        const auto selectedFiles = d.selectedFiles();
        if (!selectedFiles.isEmpty())
            res = selectedFiles.first();
    }
    return res;
}

QString ZGenericFuncs::getExistingDirectoryD ( QWidget * parent, const QString & caption,
                                               const QString & dir, QFileDialog::Options options )
{
    return QFileDialog::getExistingDirectory(parent,caption,dir,options);
}

QString ZGenericFuncs::generateUniqName(QSpinBox *counter, const QString& tmpl, const QPixmap& snapshot, const QString &dir,
                                        const QString& format, bool withoutPath)
{
    const int numberBase = 10;

    counter->setValue(counter->value() + 1);

    QString uniq = tmpl;
    if (uniq.isEmpty())
        uniq = QSL("%NN");

    QRegularExpression exp(QSL("%(\\w+)"));
    QRegularExpressionMatch mexp = exp.match(uniq);
    for (int i=0; i < mexp.lastCapturedIndex(); i++) {
        const int length = mexp.capturedLength(i);
        const int pos = mexp.capturedStart(i);
        if (length>=2) {
            const QString tl = uniq.mid(pos+1,length-1);
            if (tl.contains(QRegularExpression(QSL("N+")))) {
                uniq.replace(pos, length, QSL("%1").arg(counter->value(),tl.length(),numberBase,QChar('0')));
            } else if (tl == QSL("w")) {
                uniq.replace(pos, length, QSL("%1").arg(snapshot.width()));
            } else if (tl == QSL("h")) {
                uniq.replace(pos, length, QSL("%1").arg(snapshot.height()));
            } else if (tl == QSL("y")) {
                uniq.replace(pos, length, QDateTime::currentDateTime().toString(QSL("yyyy")));
            } else if (tl == QSL("m")) {
                uniq.replace(pos, length, QDateTime::currentDateTime().toString(QSL("MM")));
            } else if (tl == QSL("d")) {
                uniq.replace(pos, length, QDateTime::currentDateTime().toString(QSL("dd")));
            } else if (tl == QSL("t")) {
                uniq.replace(pos, length, QDateTime::currentDateTime().toString(QSL("hh-mm-ss")));
            }
        }
    }

    QDir d(dir);
    QString ext;
    if (!format.isEmpty())
        ext = QSL(".%1").arg(format.toLower());

    QFileInfo fi(d.absoluteFilePath(QSL("%1%2").arg(uniq,ext)));
    int idx = 1;
    while (fi.exists()) {
        fi.setFile(d.absoluteFilePath(QSL("%1-%3%2").arg(uniq,ext).arg(idx)));
        idx++;
    }

    if (withoutPath)
        return fi.fileName();

    return fi.absoluteFilePath();
}

QString ZGenericFuncs::generateFilter(const QStringList &ext)
{
    QString filter;

    for (int i=0;i<ext.count();i++) {
        if (i>0)
            filter.append(QSL(";;"));
        const QString itm = ext.at(i);
        filter.append(QSL("%1 (*.%2)").arg(itm.toUpper(),itm.toLower()));
    }
    return filter;
}

struct iiibiiay
{
    explicit iiibiiay(const QImage& pic);
    iiibiiay() = default;
    int width { 0 };
    int height { 0 };
    int bytesPerLine { 0 };
    bool alpha { false };
    int channels { 0 };
    int bitPerPixel { 0 };
    QByteArray data;
};
Q_DECLARE_METATYPE(iiibiiay)

iiibiiay::iiibiiay(const QImage &pic)
{
    QImage img(pic);
    if(img.format()!=QImage::Format_ARGB32)
        img = img.convertToFormat(QImage::Format_ARGB32);
    img = img.rgbSwapped();
    width = img.width();
    height = img.height();
    bytesPerLine = img.bytesPerLine();
    alpha = img.hasAlphaChannel();
    channels = (img.isGrayscale()?1:(alpha?4:3));
    bitPerPixel = img.depth()/channels;
    data.append(reinterpret_cast<const char *>(img.constBits()),
                static_cast<int>(img.sizeInBytes()));
}

QDBusArgument &operator<<(QDBusArgument &a, const iiibiiay &i)
{
    a.beginStructure();
    a << i.width <<i.height << i.bytesPerLine << i.alpha << i.bitPerPixel << i.channels << i.data;
    a.endStructure();
    return a;
}

const QDBusArgument &operator>>(const QDBusArgument &a,  iiibiiay &i)
{
    a.beginStructure();
    a >> i.width >> i.height >> i.bytesPerLine >> i.alpha >> i.bitPerPixel >> i.channels >> i.data;
    a.endStructure();
    return a;
}

void ZGenericFuncs::sendDENotification(QWidget* parent, const QString &text, const QString &title, int timeout_ms)
{
    static auto dbusID = qDBusRegisterMetaType<iiibiiay>(); // static initialization on first call, run once
    Q_UNUSED(dbusID);

    const int iconSize = 48;

    QDBusInterface dbus(QSL("org.freedesktop.Notifications"),
                        QSL("/org/freedesktop/Notifications"),
                        QSL("org.freedesktop.Notifications"),
                        QDBusConnection::sessionBus());

    quint32 replacesID = 0U;
    QVariantList args;
    args.append(QSL("scrcap"));                     // Application name
    args.append(QVariant::fromValue(replacesID));   // Replaces ID
    args.append(QVariant::fromValue(QString()));    // icon
    if (!title.isEmpty()) {
        args.append(title);                         // Message title
    } else {
        args.append(text);
    }
    args.append(text);                              // Message body
    args.append(QStringList());                     // Actions

    QVariantMap map;
    iiibiiay img(parent->windowIcon().pixmap(iconSize).toImage());
    map.insert(QSL("image-data"),QVariant::fromValue(img));

    args.append(map);                               // Hints
    args.append(timeout_ms);                        // Expiration timeout

    dbus.callWithArgumentList(QDBus::AutoDetect, QSL("Notify"), args);
}
