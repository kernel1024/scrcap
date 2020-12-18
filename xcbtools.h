#ifndef XCBTOOLS_H
#define XCBTOOLS_H

#include <QObject>
#include <QPixmap>
#include <QVector>
#include <QRect>
#include <QThread>
#include <QMutex>
#include <QHash>
#include <QPointer>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xfixes.h>

class ZAbstractXCBEventListener;

class ZXCBTools : public QObject
{
    Q_OBJECT
private:
    QMutex m_listenersMutex;
    QPointer<QThread> m_eventLoopThread;
    QHash<ZAbstractXCBEventListener *, quint8> m_eventListeners;
    xcb_connection_t* m_connection { nullptr };
    xcb_atom_t m_closeAtom { 0 };

    QThread *createEventLoop();
    void exitEventLoop();
    static bool ungrabKey(xcb_keycode_t keycode, uint16_t modifiers, xcb_window_t window);
    static bool grabKey(xcb_keycode_t keycode, uint16_t modifiers, xcb_window_t window);
public:
    explicit ZXCBTools(QObject *parent = nullptr);
    ~ZXCBTools() override;

    static ZXCBTools* instance();
    static xcb_connection_t* connection(ZXCBTools *inst);
    static void addEventListener(ZAbstractXCBEventListener *receiver, quint8 responseType);
    static void removeEventListener(ZAbstractXCBEventListener *receiver);

    static xcb_window_t appRootWindow();
    static QPixmap convertFromNative(xcb_image_t *xcbImage);
    static QRect getWindowGeometry(xcb_window_t window);
    static QPixmap getWindowPixmap(xcb_window_t window, bool blendPointer);
    static QPixmap grabCurrent(bool includeDecorations, bool includePointer, QRect *windowRegion);
    static QPixmap blendCursorImage(const QPixmap &pixmap, int x, int y, int width, int height);
    static void getWindowsRecursive( QVector<QRect> &windows, xcb_window_t w, int rx = 0, int ry = 0, int depth = 0 );
    static xcb_window_t findRealWindow( xcb_window_t w, int depth = 0 );
    static xcb_window_t windowUnderCursor( bool includeDecorations = true );

    static xcb_keycode_t nativeKeycode(Qt::Key key, Qt::KeyboardModifiers modifiers);
    static uint16_t nativeModifiers(Qt::KeyboardModifiers modifiers);
    static bool registerShortcut(xcb_keycode_t nativeKey, uint16_t nativeMods);
    static bool unregisterShortcut(xcb_keycode_t nativeKey, uint16_t nativeMods);
    static xcb_keysym_t keyToKeysym(const Qt::Key key, const Qt::KeyboardModifiers mods);
};

class ZAbstractXCBEventListener : public QObject
{
    Q_OBJECT

    friend class ZXCBTools;

public:
    explicit ZAbstractXCBEventListener(QObject* parent = nullptr) : QObject(parent) {}
    ~ZAbstractXCBEventListener() override {}

protected:
    virtual void nativeEventHandler(const xcb_generic_event_t* event) = 0;
};

#endif // XCBTOOLS_H

