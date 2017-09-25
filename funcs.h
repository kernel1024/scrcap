#ifndef FUNCS_H
#define FUNCS_H

#include <QString>
#include <QFileDialog>
#include <QStringList>
#include <QWidget>

QString getOpenFileNameD ( QWidget * parent = nullptr, const QString & caption = QString(),
                           const QString & dir = QString(), const QString & filter = QString(),
                           QString * selectedFilter = nullptr, QFileDialog::Options options = nullptr);

QString getSaveFileNameD (QWidget * parent = nullptr, const QString & caption = QString(),
                          const QString & dir = QString(), const QString & filter = QString(),
                          QString * selectedFilter = nullptr, QFileDialog::Options options = nullptr,
                          QString preselectFileName = QString());

QString getExistingDirectoryD ( QWidget * parent = nullptr, const QString & caption = QString(),
                                const QString & dir = QString(),
                                QFileDialog::Options options = QFileDialog::ShowDirsOnly);

QString generateUniqName(const QString& tmpl, const QPixmap &snapshot, const QString &dir,
                         const QString &format = QString(), const bool withoutPath = true);

QString generateFilter(const QStringList& ext);

void sendDENotification(QWidget *parent, const QString& text, const QString& title = QString(),
                        const int timeout_ms = 500);

#endif // FUNCS_H
