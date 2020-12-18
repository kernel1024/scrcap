#ifndef FUNCS_H
#define FUNCS_H

#include <QObject>
#include <QString>
#include <QFileDialog>
#include <QStringList>
#include <QWidget>

#define QSL QStringLiteral

class ZGenericFuncs : public QObject
{
    Q_OBJECT

public:
    ZGenericFuncs(QObject *parent = nullptr);
    ~ZGenericFuncs() override;

    static const QStringList &zCaptureMode();
    static const QStringList &zImageFormats();
    static QStringList getSuffixesFromFilter(const QString& filter);

    static QString getOpenFileNameD(QWidget * parent = nullptr, const QString & caption = QString(),
                                    const QString & dir = QString(), const QString & filter = QString(),
                                    QString * selectedFilter = nullptr,
                                    QFileDialog::Options options = QFileDialog::DontUseNativeDialog |
                                                                   QFileDialog::DontUseCustomDirectoryIcons);

    static QString getSaveFileNameD(QWidget * parent = nullptr, const QString & caption = QString(),
                                    const QString & dir = QString(), const QString & filter = QString(),
                                    QString * selectedFilter = nullptr,
                                    QFileDialog::Options options = QFileDialog::DontUseNativeDialog |
                                                                   QFileDialog::DontUseCustomDirectoryIcons,
                                    const QString &preselectFileName = QString());

    static QString getExistingDirectoryD(QWidget * parent = nullptr, const QString & caption = QString(),
                                         const QString & dir = QString(),
                                         QFileDialog::Options options = QFileDialog::ShowDirsOnly |
                                                                        QFileDialog::DontUseNativeDialog |
                                                                        QFileDialog::DontUseCustomDirectoryIcons);

    static QString generateUniqName(const QString& tmpl, const QPixmap &snapshot, const QString &dir,
                                    const QString &format = QString(), bool withoutPath = true);

    static QString generateFilter(const QStringList& ext);

    static void sendDENotification(QWidget *parent, const QString& text, const QString& title = QString(),
                                   int timeout_ms = 500);

};

#endif // FUNCS_H
