#ifndef GSTPLAYER_H
#define GSTPLAYER_H

#include <QObject>
#include <QUrl>

#ifdef WITH_GST
#include <gst/gst.h>
#endif

struct CStreamerData {
#ifdef WITH_GST
    GstElement *playbin { nullptr };
    guint busWatchID { 0 };
    void clear();
#else
    int dummy { 0 };
#endif
};


class ZGSTPlayer : public QObject
{
    Q_OBJECT
private:
    QUrl m_media;
    CStreamerData m_data;

public:
    explicit ZGSTPlayer(QObject *parent = nullptr);
    bool isGSTSupported() const;
    bool isPlaying() const;

    QUrl media() const;
    void setMedia(const QUrl &media);

public Q_SLOTS:
    void play();
    void stop();

Q_SIGNALS:
    void started();
    void stopped();
    void errorOccured(const QString& message);
};

#endif // GSTPLAYER_H
