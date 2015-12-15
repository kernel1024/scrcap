#include <QStringList>
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
