#include <QPointer>
#include <QMutex>
#include <QCursor>
#include <QPoint>
#include <QPainter>
#include <QScreen>
#include <QApplication>
#include <QKeySequence>
#include <QDebug>

#include <algorithm>
#include <X11/keysym.h>

#include "xcbtools.h"

static const int minSize = 8;

ZXCBTools::ZXCBTools(QObject *parent)
    : QObject(parent)
{
    const char *closeAtom = "_ZXCB_CLOSE_CONNECTION";

    m_connection = xcb_connect(nullptr, nullptr);
    int res = xcb_connection_has_error(m_connection);
    if (res>0) {
        qCritical() << "Error in XCB connection " << res;
        return;
    }

    xcb_intern_atom_cookie_t ac = xcb_intern_atom(m_connection, 0, strlen(closeAtom), closeAtom);
    QScopedPointer<xcb_intern_atom_reply_t,QScopedPointerPodDeleter>
            acr(xcb_intern_atom_reply(m_connection, ac, nullptr));
    if (acr)
        m_closeAtom = acr->atom;

    m_eventLoopThread = createEventLoop();
    connect(m_eventLoopThread.data(),&QThread::finished,m_eventLoopThread.data(),&QThread::deleteLater);
    m_eventLoopThread->setObjectName(QStringLiteral("ZXCBTools"));
    m_eventLoopThread->start();
}

ZXCBTools::~ZXCBTools()
{
    if (m_eventLoopThread)
        exitEventLoop();

    xcb_disconnect(m_connection);
}

void ZXCBTools::addEventListener(ZAbstractXCBEventListener *receiver, quint8 responseType)
{
    auto* inst = ZXCBTools::instance();
    QMutexLocker locker(&(inst->m_listenersMutex));
    inst->m_eventListeners.insert(receiver,responseType);
}

void ZXCBTools::removeEventListener(ZAbstractXCBEventListener *receiver)
{
    auto* inst = ZXCBTools::instance();
    QMutexLocker locker(&(inst->m_listenersMutex));
    inst->m_eventListeners.remove(receiver);
}

QThread* ZXCBTools::createEventLoop()
{
    QThread* res = QThread::create([this](){
        const unsigned short responseMask = 0x7f;

        for (;;) {
            QScopedPointer<xcb_generic_event_t,QScopedPointerPodDeleter> event
                    (xcb_wait_for_event(m_connection));

            if (event.isNull()) break;
            const quint8 responseType = event->response_type & responseMask;

            // exit loop on client message with our exit-atom
            if (responseType == XCB_CLIENT_MESSAGE) {
                const auto *clientMessage = reinterpret_cast<const xcb_client_message_event_t *>(event.data());
                if (clientMessage->type == m_closeAtom)
                    return;
            }

            m_listenersMutex.lock();
            for (auto it = m_eventListeners.constKeyValueBegin(), end = m_eventListeners.constKeyValueEnd();
                 it != end; ++it) {
                if (responseType == (*it).second) {
                    (*it).first->nativeEventHandler(event.data());
                }
            }
            m_listenersMutex.unlock();
        }
    });
    return res;
}

void ZXCBTools::exitEventLoop()
{
    if (m_eventLoopThread.isNull()) return;

    // A hack to close XCB connection. Apparently XCB does not have any APIs for this?
    xcb_client_message_event_t event {};
    memset(&event, 0, sizeof(event));
    const xcb_setup_t* setup = xcb_get_setup(m_connection);
    const xcb_window_t window = xcb_generate_id(m_connection);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);

    if (it.rem>0) {
        const unsigned short msgFormat = 32; // undocumented by XCB
        xcb_screen_t *screen = it.data;
        xcb_create_window(m_connection, XCB_COPY_FROM_PARENT,
                          window, screen->root,
                          0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY,
                          screen->root_visual, 0, nullptr);
        event.response_type = XCB_CLIENT_MESSAGE;
        event.format = msgFormat;
        event.sequence = 0;
        event.window = window;
        event.type = m_closeAtom;
        event.data.data32[0] = 0;
        xcb_send_event(m_connection, 0, window, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char *>(&event));
        xcb_destroy_window(m_connection, window);
        xcb_flush(m_connection);
    } else {
        qCritical() << "Unable to stop auxiliary XCB event loop in order. Aborting XCB aux thread by force.";
        m_eventLoopThread->terminate();
    }

    // Wait for event thread termination
    m_eventLoopThread->wait();
}

ZXCBTools *ZXCBTools::instance()
{
    static QPointer<ZXCBTools> inst;
    static QMutex instMutex;

    instMutex.lock();

    if (inst.isNull())
        inst = new ZXCBTools(QApplication::instance());

    instMutex.unlock();

    return inst.data();
}

xcb_connection_t *ZXCBTools::connection(ZXCBTools* inst)
{
    return inst->m_connection;
}

xcb_window_t ZXCBTools::appRootWindow()
{
    xcb_connection_t *c = connection(ZXCBTools::instance());
    const xcb_setup_t *setup = xcb_get_setup(c);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);

    while (it.rem>0) {
        xcb_window_t root = it.data->root;
        xcb_query_pointer_cookie_t pc = xcb_query_pointer(c, root);
        QScopedPointer<xcb_query_pointer_reply_t,QScopedPointerPodDeleter>
                pr(xcb_query_pointer_reply(c, pc, nullptr));
        if ((pr != nullptr) && (pr->same_screen > 0))
            return root;

        xcb_screen_next(&it);
    }

    return 0;
}

QPixmap ZXCBTools::convertFromNative(xcb_image_t *xcbImage)
{
    QImage::Format format = QImage::Format_Invalid;
    quint32 *pixels = nullptr;

    switch (xcbImage->depth) {
        case 1:
            format = QImage::Format_MonoLSB;
            break;
        case 16: // NOLINT
            format = QImage::Format_RGB16;
            break;
        case 24: // NOLINT
            format = QImage::Format_RGB32;
            break;
        case 30: // NOLINT
            // Qt doesn't have a matching image format. We need to convert manually
            pixels = reinterpret_cast<quint32 *>(xcbImage->data);
            for (uint i = 0; i < (xcbImage->size / 4); i++) {
                int r = (pixels[i] >> 22) & 0xff; // NOLINT
                int g = (pixels[i] >> 12) & 0xff; // NOLINT
                int b = (pixels[i] >>  2) & 0xff; // NOLINT

                pixels[i] = qRgba(r, g, b, 0xff); // NOLINT
            }
            // fall through, Qt format is still Format_ARGB32_Premultiplied
            [[clang::fallthrough]];
        case 32: // NOLINT
            format = QImage::Format_ARGB32_Premultiplied;
            break;
        default:
            return QPixmap(); // we don't know
    }

    QImage image(xcbImage->data, xcbImage->width, xcbImage->height, format);

    if (image.isNull()) {
        return QPixmap();
    }

    // work around an abort in QImage::color

    if (image.format() == QImage::Format_MonoLSB) {
        image.setColorCount(2);
        image.setColor(0, QColor(Qt::white).rgb());
        image.setColor(1, QColor(Qt::black).rgb());
    }

    // done

    return QPixmap::fromImage(image);
}

QRect ZXCBTools::getWindowGeometry(xcb_window_t window)
{
    QRect res;

    xcb_connection_t *xcbConn = connection(ZXCBTools::instance());

    xcb_get_geometry_cookie_t geomCookie = xcb_get_geometry_unchecked(xcbConn, window);
    QScopedPointer<xcb_get_geometry_reply_t,QScopedPointerPodDeleter>
            geomReply(xcb_get_geometry_reply(xcbConn,geomCookie, nullptr));

    if (geomReply)
        res = QRect(geomReply->x,geomReply->y,geomReply->width,geomReply->height);

    return res;
}

QPixmap ZXCBTools::getWindowPixmap(xcb_window_t window, bool blendPointer)
{
    xcb_connection_t *xcbConn = connection(ZXCBTools::instance());

    // first get geometry information for our drawable

    xcb_get_geometry_cookie_t geomCookie = xcb_get_geometry_unchecked(xcbConn, window);
    QScopedPointer<xcb_get_geometry_reply_t,QScopedPointerPodDeleter>
            geomReply(xcb_get_geometry_reply(xcbConn, geomCookie, nullptr));

    // then proceed to get an image

    if (geomReply.isNull()) return QPixmap();

    QScopedPointer<xcb_image_t,QScopedPointerPodDeleter>
            xcbImage(xcb_image_get(xcbConn,
                                   window,
                                   geomReply->x,
                                   geomReply->y,
                                   geomReply->width,
                                   geomReply->height,
                                   ~0U,
                                   XCB_IMAGE_FORMAT_Z_PIXMAP
                                   ));


    // if the image is null, this means we need to get the root image window
    // and run a crop

    if (!xcbImage) {
        QRect geom(geomReply->x, geomReply->y, geomReply->width, geomReply->height);
        return getWindowPixmap(appRootWindow(), blendPointer).copy(geom);
    }

    // now process the image

    QPixmap nativePixmap = convertFromNative(xcbImage.data());
    if (!(blendPointer))
        return nativePixmap;

    // now we blend in a pointer image

    xcb_get_geometry_cookie_t geomRootCookie = xcb_get_geometry_unchecked(xcbConn, geomReply->root);
    QScopedPointer<xcb_get_geometry_reply_t,QScopedPointerPodDeleter>
            geomRootReply(xcb_get_geometry_reply(xcbConn, geomRootCookie, nullptr));

    xcb_translate_coordinates_cookie_t translateCookie = xcb_translate_coordinates_unchecked(
                                                             xcbConn, window, geomReply->root, geomRootReply->x, geomRootReply->y);
    QScopedPointer<xcb_translate_coordinates_reply_t,QScopedPointerPodDeleter>
            translateReply(xcb_translate_coordinates_reply(xcbConn, translateCookie, nullptr));

    return blendCursorImage(nativePixmap, translateReply->dst_x,translateReply->dst_y,
                            geomReply->width, geomReply->height);
}

QPixmap ZXCBTools::grabCurrent(bool includeDecorations, bool includePointer, QRect *windowRegion)
{
    xcb_connection_t* c = connection(ZXCBTools::instance());


    xcb_window_t child = windowUnderCursor(includeDecorations);

    xcb_query_tree_cookie_t tc = xcb_query_tree_unchecked(c, child);
    QScopedPointer<xcb_query_tree_reply_t,QScopedPointerPodDeleter> tree(xcb_query_tree_reply(c, tc, nullptr));

    QRect geom = getWindowGeometry(child);
    if (!geom.isNull()) {
        if (tree) {
            xcb_translate_coordinates_cookie_t tc = xcb_translate_coordinates(
                                                        c, tree->parent, appRootWindow(),
                                                        geom.x(), geom.y());
            QScopedPointer<xcb_translate_coordinates_reply_t,QScopedPointerPodDeleter> tr
                    (xcb_translate_coordinates_reply(c, tc, nullptr));

            if (tr) {
                geom.setX(tr->dst_x);
                geom.setY(tr->dst_y);
            }
        }
        *windowRegion = geom;
    }

    QPixmap pm(getWindowPixmap(child, includePointer));
    return pm;
}

QPixmap ZXCBTools::blendCursorImage(const QPixmap &pixmap, int x, int y, int width, int height)
{
    // first we get the cursor position, compute the co-ordinates of the region
    // of the screen we're grabbing, and see if the cursor is actually visible in
    // the region

    QPoint cursorPos = QCursor::pos();
    QRect screenRect(x, y, width, height);

    if (!(screenRect.contains(cursorPos))) {
        return pixmap;
    }

    // now we can get the image and start processing

    xcb_connection_t *xcbConn = connection(ZXCBTools::instance());

    xcb_xfixes_get_cursor_image_cookie_t  cursorCookie = xcb_xfixes_get_cursor_image_unchecked(xcbConn);
    QScopedPointer<xcb_xfixes_get_cursor_image_reply_t,QScopedPointerPodDeleter>
            cursorReply(xcb_xfixes_get_cursor_image_reply(xcbConn, cursorCookie, nullptr));

    if (!cursorReply)
        return pixmap;

    quint32 *pixelData = xcb_xfixes_get_cursor_image_cursor_image(cursorReply.data());
    if (!pixelData)
        return pixmap;

    // process the image into a QImage

    QImage cursorImage = QImage(reinterpret_cast<quint8 *>(pixelData),
                                cursorReply->width, cursorReply->height,
                                QImage::Format_ARGB32_Premultiplied);

    // a small fix for the cursor position for fancier cursors

    cursorPos -= QPoint(cursorReply->xhot, cursorReply->yhot);

    // now we translate the cursor point to our screen rectangle

    cursorPos -= QPoint(x, y);

    // and do the painting

    QPixmap blendedPixmap = pixmap;
    QPainter painter(&blendedPixmap);
    painter.drawImage(cursorPos, cursorImage);

    // and done

    return blendedPixmap;
}

// Recursively iterates over the window w and its children, thereby building
// a tree of window descriptors. Windows in non-viewable state or with height
// or width smaller than minSize will be ignored.
void ZXCBTools::getWindowsRecursive( QVector<QRect> &windows, xcb_window_t w, int rx, int ry, int depth )
{
    xcb_connection_t* c = connection(ZXCBTools::instance());

    xcb_get_window_attributes_cookie_t ac = xcb_get_window_attributes_unchecked(c, w);
    QScopedPointer<xcb_get_window_attributes_reply_t,QScopedPointerPodDeleter>
            atts(xcb_get_window_attributes_reply(c, ac, nullptr));

    xcb_get_geometry_cookie_t gc = xcb_get_geometry_unchecked(c, w);
    QScopedPointer<xcb_get_geometry_reply_t,QScopedPointerPodDeleter>
            geom(xcb_get_geometry_reply(c, gc, nullptr));

    if ( atts && geom &&
         atts->map_state == XCB_MAP_STATE_VIEWABLE &&
         geom->width >= minSize && geom->height >= minSize ) {
        int x = 0;
        int y = 0;
        if ( depth != 0 ) {
            x = geom->x + rx;
            y = geom->y + ry;
        }

        QRect r( x, y, geom->width, geom->height );
        if (!windows.contains(r))
            windows.append(r);

        xcb_query_tree_cookie_t tc = xcb_query_tree_unchecked(c, w);
        QScopedPointer<xcb_query_tree_reply_t,QScopedPointerPodDeleter>
                tree(xcb_query_tree_reply(c, tc, nullptr));

        if (tree) {
            xcb_window_t* child = xcb_query_tree_children(tree.data());
            for (unsigned int i=0;i<tree->children_len;i++)
                getWindowsRecursive(windows, child[i], x, y, depth +1); // NOLINT
        }
    }

    if ( depth == 0 ) {
        std::sort(windows.begin(), windows.end(), []( const QRect& r1, const QRect& r2 )
        {
            return r1.width() * r1.height() < r2.width() * r2.height();
        });
    }
}

xcb_window_t ZXCBTools::findRealWindow( xcb_window_t w, int depth )
{
    const char *wm_state_s = "WM_STATE";

    xcb_connection_t* c = connection(ZXCBTools::instance());

    if( depth > 5 )
        return 0;

    xcb_intern_atom_cookie_t ac = xcb_intern_atom(c, 0, strlen(wm_state_s), wm_state_s);
    QScopedPointer<xcb_intern_atom_reply_t,QScopedPointerPodDeleter>
            wm_state(xcb_intern_atom_reply(c, ac, nullptr));

    if (wm_state.isNull()) {
        qWarning() << "Unable to allocate xcb atom";
        return 0;
    }

    xcb_get_property_cookie_t pc = xcb_get_property(c, 0, w, wm_state->atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 0 );
    QScopedPointer<xcb_get_property_reply_t,QScopedPointerPodDeleter>
            pr(xcb_get_property_reply(c, pc, nullptr));

    if (pr && pr->type != XCB_NONE)
        return w;

    xcb_window_t ret = XCB_NONE;

    xcb_query_tree_cookie_t tc = xcb_query_tree_unchecked(c, w);
    QScopedPointer<xcb_query_tree_reply_t,QScopedPointerPodDeleter>
            tree(xcb_query_tree_reply(c, tc, nullptr));

    if (tree) {
        xcb_window_t* child = xcb_query_tree_children(tree.data());
        for (unsigned int i=0;i<tree->children_len;i++)
            ret = findRealWindow(child[i], depth +1 ); // NOLINT
    }

    return ret;
}

xcb_window_t ZXCBTools::windowUnderCursor( bool includeDecorations )
{
    xcb_connection_t* c = connection(ZXCBTools::instance());

    xcb_window_t child = appRootWindow();

    xcb_query_pointer_cookie_t pc = xcb_query_pointer(c,appRootWindow());
    QScopedPointer<xcb_query_pointer_reply_t,QScopedPointerPodDeleter>
            pr(xcb_query_pointer_reply(c, pc, nullptr));

    if (pr && pr->child!=XCB_NONE )
        child = pr->child;

    if( !includeDecorations ) {
        xcb_window_t real_child = findRealWindow( child );

        if( real_child != XCB_NONE ) { // test just in case
            child = real_child;
        }
    }

    return child;
}

bool ZXCBTools::ungrabKey(xcb_keycode_t keycode, uint16_t modifiers, xcb_window_t window)
{
    xcb_connection_t* c = connection(ZXCBTools::instance());
    if (c==nullptr) return false;

    xcb_void_cookie_t vc = xcb_ungrab_key(c, keycode, window, modifiers);

    return (xcb_request_check(c, vc)==nullptr);
}

bool ZXCBTools::grabKey(xcb_keycode_t keycode, uint16_t modifiers, xcb_window_t window)
{
    xcb_connection_t* c = connection(ZXCBTools::instance());
    if (c==nullptr) return false;

    xcb_void_cookie_t vc = xcb_grab_key(c, 1, window, modifiers, keycode,
                                        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

    if (xcb_request_check(c, vc)) {
        ungrabKey(keycode, modifiers, window);
        return false;
    }

    return true;
}

xcb_keysym_t ZXCBTools::keyToKeysym(const Qt::Key key, const Qt::KeyboardModifiers mods)
{
    unsigned int code = 0;
    switch( key )
    {
        // modifiers are handled separately
        case Qt::Key_Shift: code = XK_Shift_L; break;
        case Qt::Key_Control: code = XK_Control_L; break;
        case Qt::Key_Meta: code = XK_Meta_L; break;
        case Qt::Key_Alt: code = XK_Alt_L; break;
        case Qt::Key_Escape: code = XK_Escape; break;
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            code = XK_Tab;
            break;
        case Qt::Key_Backspace: code = XK_BackSpace; break;
        case Qt::Key_Return: code = XK_Return; break;
        case Qt::Key_Insert: code = XK_Insert; break;
        case Qt::Key_Delete: code = XK_Delete; break;
        case Qt::Key_Pause: code = XK_Pause; break;
        case Qt::Key_Print: code = XK_Print; break;
        case Qt::Key_Home: code = XK_Home; break;
        case Qt::Key_End: code = XK_End; break;
        case Qt::Key_Left: code = XK_Left; break;
        case Qt::Key_Up: code = XK_Up; break;
        case Qt::Key_Right: code = XK_Right; break;
        case Qt::Key_Down: code = XK_Down; break;
        case Qt::Key_PageUp: code = XK_Prior; break;
        case Qt::Key_PageDown: code = XK_Next; break;
        case Qt::Key_CapsLock: code = XK_Caps_Lock; break;
        case Qt::Key_NumLock: code = XK_Num_Lock; break;
        case Qt::Key_ScrollLock: code = XK_Scroll_Lock; break;
        case Qt::Key_Super_L: code = XK_Super_L; break;
        case Qt::Key_Super_R: code = XK_Super_R; break;
        case Qt::Key_Menu: code = XK_Menu; break;
        case Qt::Key_Hyper_L: code = XK_Hyper_L; break;
        case Qt::Key_Hyper_R: code = XK_Hyper_R; break;
        case Qt::Key_Help: code = XK_Help; break;
        case Qt::Key_AltGr: code = XK_ISO_Level3_Shift; break;
        case Qt::Key_Multi_key: code = XK_Multi_key; break;
        case Qt::Key_SingleCandidate: code = XK_SingleCandidate; break;
        case Qt::Key_MultipleCandidate: code = XK_MultipleCandidate; break;
        case Qt::Key_PreviousCandidate: code = XK_PreviousCandidate; break;
        case Qt::Key_Mode_switch: code = XK_Mode_switch; break;
        default: break;
    }

    if( key >= Qt::Key_F1 && key <= Qt::Key_F35 )
    {
        code = XK_F1 + key - Qt::Key_F1;
    }

    if ( code == 0 ) {
        QKeySequence seq(key);
        if (((mods & Qt::ControlModifier) != 0U) && seq.toString().length() == 1) {
            QString s = seq.toString();
            if ((mods & Qt::ShiftModifier) == 0U)
                s = s.toLower();

            code = s.front().unicode();
        } else if (seq.toString().length() == 1){
            code = seq.toString().front().unicode();
        }
    }

    return code;
}

xcb_keycode_t ZXCBTools::nativeKeycode(Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    xcb_keycode_t ret = 0;

    if (key==0) return ret;

    xcb_connection_t* c = connection(ZXCBTools::instance());
    if (c==nullptr) return ret;

    xcb_keysym_t sym = keyToKeysym(key, modifiers);

    xcb_key_symbols_t *syms = xcb_key_symbols_alloc(c);
    if (syms) {
        QScopedPointer<xcb_keycode_t,QScopedPointerPodDeleter> keyCodes(xcb_key_symbols_get_keycode(syms, sym));
        if (keyCodes)
            ret = *(keyCodes.data());
        xcb_key_symbols_free(syms);
    }

    return ret;
}

uint16_t ZXCBTools::nativeModifiers(Qt::KeyboardModifiers modifiers)
{
    // ShiftMask, LockMask, ControlMask, Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, and Mod5Mask
    uint16_t native = 0;
    if ((modifiers & Qt::ShiftModifier) != 0U)
        native |= XCB_MOD_MASK_SHIFT;
    if ((modifiers & Qt::ControlModifier) != 0U)
        native |= XCB_MOD_MASK_CONTROL;
    if ((modifiers & Qt::AltModifier) != 0U)
        native |= XCB_MOD_MASK_1;
    if ((modifiers & Qt::MetaModifier) != 0U)
        native |= XCB_MOD_MASK_4;

    return native;
}

bool ZXCBTools::registerShortcut(xcb_keycode_t nativeKey, uint16_t nativeMods)
{
    return grabKey(nativeKey, nativeMods, static_cast<unsigned int>(appRootWindow()));
}

bool ZXCBTools::unregisterShortcut(xcb_keycode_t nativeKey, uint16_t nativeMods)
{
    return ungrabKey(nativeKey, nativeMods, static_cast<unsigned int>(appRootWindow()));
}
