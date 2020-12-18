#include <QDebug>
#include "gstplayer.h"
#include "funcs.h"

ZGSTPlayer::ZGSTPlayer(QObject *parent)
    : QObject(parent)
{
#ifdef WITH_GST
    gst_init(nullptr,nullptr);
#endif
}

bool ZGSTPlayer::isGSTSupported() const
{
#ifdef WITH_GST
    return true;
#else
    return false;
#endif
}

bool ZGSTPlayer::isPlaying() const
{
#ifdef WITH_GST
    return (m_data.playbin != nullptr);
#else
    return false;
#endif
}

QUrl ZGSTPlayer::media() const
{
    return m_media;
}

void ZGSTPlayer::setMedia(const QUrl &media)
{
    m_media = media;
}

#ifdef WITH_GST
gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
    Q_UNUSED(bus)
    auto *dlg = qobject_cast<ZGSTPlayer *>(reinterpret_cast<QObject *>(data));

    gchar  *debug = nullptr;
    GError *error = nullptr;
    QString prefix = QSL("AUX");
    QString message;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            if (dlg) {
                QMetaObject::invokeMethod(dlg,[dlg](){
                    dlg->stop();
                },Qt::QueuedConnection);
            }
            break;
        case GST_MESSAGE_ERROR:   gst_message_parse_error(msg, &error, &debug); prefix = QSL("ERROR"); break;
        case GST_MESSAGE_WARNING: gst_message_parse_warning(msg,&error,&debug); prefix = QSL("WARN"); break;
        case GST_MESSAGE_INFO:    gst_message_parse_info(msg, &error, &debug);  prefix = QSL("INFO"); break;
        default: break;
    }

    if (debug)
        g_free(debug);
    if (error) {
        message = QSL("GStreamer Bus %1: %2").arg(prefix,QString::fromUtf8(error->message));
        g_error_free (error);
    }

    if (!message.isEmpty())
        qWarning() << message;

    return TRUE;
}
#endif

void ZGSTPlayer::play()
{
    if (isPlaying())
        stop();

#ifdef WITH_GST
    const guint GST_PLAY_FLAG_VIDEO = 0x001;
    const guint GST_PLAY_FLAG_AUDIO = 0x002;
    const guint GST_PLAY_FLAG_TEXT = 0x004;

    m_data.playbin = gst_element_factory_make("playbin", "playbin");

    if (!m_data.playbin) {
        qCritical() << "GStreamer: Not all elements could be created.";
        return;
    }

    /* Set the URI to play */
    const QByteArray uri = m_media.toEncoded();
    g_object_set(m_data.playbin, "uri", uri.constData(), nullptr); // NOLINT

    /* Set flags to show Audio, ignore Video, Subtitles */
    guint flags = 0U;
    g_object_get(m_data.playbin, "flags", &flags, nullptr); // NOLINT
    flags |= GST_PLAY_FLAG_AUDIO;
    flags &= ~(GST_PLAY_FLAG_TEXT | GST_PLAY_FLAG_VIDEO);
    g_object_set(m_data.playbin, "flags", flags, nullptr); // NOLINT

    /* Add a bus watch, so we get notified when a message arrives */
    GstBus *bus = gst_element_get_bus(m_data.playbin);
    m_data.busWatchID = gst_bus_add_watch(bus, bus_call, this);
    gst_object_unref(bus);

    /* Start playing */
    GstStateChangeReturn ret = gst_element_set_state(m_data.playbin, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        stop();
        qCritical() << "Unable to set the pipeline to the playing state.";
        return;
    }

    Q_EMIT started();
#endif
}

void ZGSTPlayer::stop()
{
#ifdef WITH_GST
    if (isPlaying()) {
        gst_element_set_state(m_data.playbin, GST_STATE_NULL);

        gst_object_unref(GST_OBJECT(m_data.playbin)); // NOLINT

        if (m_data.busWatchID>0)
            g_source_remove(m_data.busWatchID);

        m_data.clear();

        Q_EMIT stopped();
    }
#endif
}

#ifdef WITH_GST
void CStreamerData::clear()
{
    playbin = nullptr;
    busWatchID = 0;
}
#endif
