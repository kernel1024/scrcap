#ifndef FUNCS_H
#define FUNCS_H

#include <QString>
#include <QFileDialog>
#include <QWidget>

QString getSaveFileNameD (QWidget * parent = 0, const QString & caption = QString(),
                          const QString & dir = QString(), const QString & filter = QString(),
                          QString * selectedFilter = 0, QFileDialog::Options options = 0,
                          QString preselectFileName = QString());

QString getExistingDirectoryD ( QWidget * parent = 0, const QString & caption = QString(),
                                const QString & dir = QString(),
                                QFileDialog::Options options = QFileDialog::ShowDirsOnly);

#endif // FUNCS_H
