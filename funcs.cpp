#include <QDir>
#include <QDateTime>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QWidget>
#include <QIcon>
#include <QMediaPlayer>
#include <QUrl>

#include "funcs.h"

QStringList getSuffixesFromFilter(const QString& filter)
{
    QStringList res;
    res.clear();
    if (filter.isEmpty()) return res;

    res = filter.split(";;",QString::SkipEmptyParts);
    if (res.isEmpty()) return res;
    QString ex = res.first();
    res.clear();

    if (ex.isEmpty()) return res;

    ex.remove(QRegExp("^.*\\("));
    ex.remove(QRegExp("\\).*$"));
    ex.remove(QRegExp("^.*\\."));
    res = ex.split(" ");

    return res;
}

QString getOpenFileNameD ( QWidget * parent, const QString & caption, const QString & dir,
                           const QString & filter, QString * selectedFilter,
                           QFileDialog::Options options )
{
    return QFileDialog::getOpenFileName(parent,caption,dir,filter,selectedFilter,options);
}

QString getSaveFileNameD ( QWidget * parent, const QString & caption, const QString & dir,
                           const QString & filter, QString * selectedFilter, QFileDialog::Options options,
                           QString preselectFileName )
{
    QFileDialog d(parent,caption,dir,filter);
    d.setFileMode(QFileDialog::AnyFile);
    d.setOptions(options);
    d.setAcceptMode(QFileDialog::AcceptSave);

    QStringList sl;
    if (selectedFilter!=NULL && !selectedFilter->isEmpty())
        sl=getSuffixesFromFilter(*selectedFilter);
    else
        sl=getSuffixesFromFilter(filter);
    if (!sl.isEmpty())
        d.setDefaultSuffix(sl.first());

    if (selectedFilter && !selectedFilter->isEmpty())
            d.selectNameFilter(*selectedFilter);

    if (!preselectFileName.isEmpty())
        d.selectFile(preselectFileName);

    if (d.exec()==QDialog::Accepted) {
        if (selectedFilter!=NULL)
            *selectedFilter=d.selectedNameFilter();
        if (!d.selectedFiles().isEmpty())
            return d.selectedFiles().first();
        else
            return QString();
    } else
        return QString();
}

QString getExistingDirectoryD ( QWidget * parent, const QString & caption,
                                const QString & dir, QFileDialog::Options options )
{
    return QFileDialog::getExistingDirectory(parent,caption,dir,options);
}

static int fileCounter = 0;

QString generateUniqName(const QString& tmpl, const QPixmap& snapshot, const QString &dir,
                         const QString& format, const bool withoutPath)
{
    fileCounter++;

    QString uniq = tmpl;
    if (uniq.isEmpty())
        uniq = "%NN";

    QRegExp exp("%\\S+");
    int pos = 0;
    while ((pos=exp.indexIn(uniq,pos)) != -1) {
        int length = exp.matchedLength();
        if (length>=2) {
            QString tl = uniq.mid(pos+1,length-1);
            if (tl.contains(QRegExp("N+")))
                uniq.replace(pos, length, QString("%1").arg(fileCounter,tl.length(),10,QChar('0')));
            else if (tl == "w")
                uniq.replace(pos, length, QString("%1").arg(snapshot.width()));
            else if (tl == "h")
                uniq.replace(pos, length, QString("%1").arg(snapshot.height()));
            else if (tl == "y")
                uniq.replace(pos, length, QDateTime::currentDateTime().toString("yyyy"));
            else if (tl == "m")
                uniq.replace(pos, length, QDateTime::currentDateTime().toString("MM"));
            else if (tl == "d")
                uniq.replace(pos, length, QDateTime::currentDateTime().toString("dd"));
            else if (tl == "t")
                uniq.replace(pos, length, QDateTime::currentDateTime().toString("hh-mm-ss"));
        }
        pos += length;
    }

    QDir d(dir);
    QString ext;
    if (!format.isEmpty())
        ext = QString(".%1").arg(format.toLower());

    QFileInfo fi(d.absoluteFilePath(QString("%1%2").arg(uniq,ext)));
    int idx = 1;
    while (fi.exists()) {
        fi.setFile(d.absoluteFilePath(QString("%1-%3%2").arg(uniq,ext).arg(idx)));
        idx++;
    }

    if (withoutPath)
        return fi.fileName();

    return fi.absoluteFilePath();
}

QString generateFilter(const QStringList &ext)
{
    QString filter;
    filter.clear();

    for (int i=0;i<ext.count();i++) {
        if (i>0)
            filter+=QString(";;");
        QString itm = ext.at(i);
        filter+=QString("%1 (*.%2)").arg(itm.toUpper(),itm.toLower());
    }
    return filter;
}

struct iiibiiay
{
    iiibiiay(const QImage& pic);
    iiibiiay();
    static const int tid;
    int width;
    int height;
    int bytesPerLine;
    bool alpha;
    int channels;
    int bitPerPixel;
    QByteArray data;
};
Q_DECLARE_METATYPE(iiibiiay)

const int iiibiiay::tid = qDBusRegisterMetaType<iiibiiay>();

iiibiiay::iiibiiay()
{
}

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
    data.append((char*)img.constBits(),img.byteCount());
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

void sendDENotification(QWidget* parent, const QString &text, const QString &title,
                        const int timeout_ms)
{
    QDBusInterface dbus("org.freedesktop.Notifications",
                        "/org/freedesktop/Notifications",
                        "org.freedesktop.Notifications",
                        QDBusConnection::sessionBus());

    QVariantList args;
    args << QString("scrcap");          // Application name
    args << QVariant(QVariant::UInt);   // Replaces ID
    args << QVariant("");               // icon
    if (!title.isEmpty())
        args << QString(title);         // Message title
    else
        args << QString(text);
    args << QString(text);              // Message body
    args << QStringList();              // Actions

    QVariantMap map;
    iiibiiay img(parent->windowIcon().pixmap(48).toImage());
    map.insert("image-data",QVariant(iiibiiay::tid,&img));

    args << map;                        // Hints
    args << timeout_ms;                 // Expiration timeout

    dbus.callWithArgumentList(QDBus::AutoDetect, "Notify", args);
}
