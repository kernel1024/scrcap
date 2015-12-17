#include <QCursor>
#include <QPoint>
#include <QPainter>
#include <QScreen>
#include <QX11Info>
#include <QDebug>

#include "xcbtools.h"

static const int minSize = 8;

QPixmap convertFromNative(xcb_image_t *xcbImage)
{
    QImage::Format format = QImage::Format_Invalid;

    switch (xcbImage->depth) {
    case 1:
        format = QImage::Format_MonoLSB;
        break;
    case 16:
        format = QImage::Format_RGB16;
        break;
    case 24:
        format = QImage::Format_RGB32;
        break;
    case 30: {
        // Qt doesn't have a matching image format. We need to convert manually
        quint32 *pixels = reinterpret_cast<quint32 *>(xcbImage->data);
        for (uint i = 0; i < (xcbImage->size / 4); i++) {
            int r = (pixels[i] >> 22) & 0xff;
            int g = (pixels[i] >> 12) & 0xff;
            int b = (pixels[i] >>  2) & 0xff;

            pixels[i] = qRgba(r, g, b, 0xff);
        }
        // fall through, Qt format is still Format_ARGB32_Premultiplied
    }
    case 32:
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

bool getWindowGeometry(xcb_window_t window, int &x, int &y, int &w, int &h)
{
    xcb_connection_t *xcbConn = QX11Info::connection();

    xcb_get_geometry_cookie_t geomCookie = xcb_get_geometry_unchecked(xcbConn, window);
    xcb_get_geometry_reply_t* geomReply = xcb_get_geometry_reply(xcbConn, geomCookie, NULL);

    if (geomReply) {
        x = geomReply->x;
        y = geomReply->y;
        w = geomReply->width;
        h = geomReply->height;
        free(geomReply);
        return true;
    } else {
        x = 0;
        y = 0;
        w = 0;
        h = 0;
        return false;
    }
}

QPixmap getWindowPixmap(xcb_window_t window, bool blendPointer)
{
    xcb_connection_t *xcbConn = QX11Info::connection();

    // first get geometry information for our drawable

    xcb_get_geometry_cookie_t geomCookie = xcb_get_geometry_unchecked(xcbConn, window);
    xcb_get_geometry_reply_t* geomReply = xcb_get_geometry_reply(xcbConn, geomCookie, NULL);

    // then proceed to get an image

    if (!geomReply) return QPixmap();

    xcb_image_t* xcbImage = xcb_image_get(
                                xcbConn,
                                window,
                                geomReply->x,
                                geomReply->y,
                                geomReply->width,
                                geomReply->height,
                                ~0,
                                XCB_IMAGE_FORMAT_Z_PIXMAP
                                );


    // if the image is null, this means we need to get the root image window
    // and run a crop

    if (!xcbImage) {
        free(geomReply);
        return getWindowPixmap(QX11Info::appRootWindow(), blendPointer)
                .copy(geomReply->x, geomReply->y, geomReply->width, geomReply->height);
    }

    // now process the image

    QPixmap nativePixmap = convertFromNative(xcbImage);
    if (!(blendPointer)) {
        free(geomReply);
        free(xcbImage);
        return nativePixmap;
    }

    // now we blend in a pointer image

    xcb_get_geometry_cookie_t geomRootCookie = xcb_get_geometry_unchecked(xcbConn, geomReply->root);
    xcb_get_geometry_reply_t* geomRootReply = xcb_get_geometry_reply(xcbConn, geomRootCookie, NULL);

    xcb_translate_coordinates_cookie_t translateCookie = xcb_translate_coordinates_unchecked(
        xcbConn, window, geomReply->root, geomRootReply->x, geomRootReply->y);
    xcb_translate_coordinates_reply_t* translateReply = xcb_translate_coordinates_reply(
                                                            xcbConn, translateCookie, NULL);

    free(geomRootReply);
    free(translateReply);
    free(geomReply);
    free(xcbImage);

    return blendCursorImage(nativePixmap, translateReply->dst_x,translateReply->dst_y,
                            geomReply->width, geomReply->height);
}

QPixmap blendCursorImage(const QPixmap &pixmap, int x, int y, int width, int height)
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

    xcb_connection_t *xcbConn = QX11Info::connection();

    xcb_xfixes_get_cursor_image_cookie_t  cursorCookie = xcb_xfixes_get_cursor_image_unchecked(xcbConn);
    xcb_xfixes_get_cursor_image_reply_t* cursorReply = xcb_xfixes_get_cursor_image_reply(
                                                           xcbConn, cursorCookie, NULL);
    if (!cursorReply) {
        return pixmap;
    }

    quint32 *pixelData = xcb_xfixes_get_cursor_image_cursor_image(cursorReply);
    if (!pixelData) {
        return pixmap;
    }

    // process the image into a QImage

    QImage cursorImage = QImage((quint8 *)pixelData, cursorReply->width, cursorReply->height,
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
    free(cursorReply);

    return blendedPixmap;
}

static bool lessThan ( const QRect& r1, const QRect& r2 )
{
    return r1.width() * r1.height() < r2.width() * r2.height();
}

// Recursively iterates over the window w and its children, thereby building
// a tree of window descriptors. Windows in non-viewable state or with height
// or width smaller than minSize will be ignored.
void getWindowsRecursive( QVector<QRect> &windows, xcb_window_t w, int rx, int ry, int depth )
{
    xcb_connection_t* c = QX11Info::connection();

    xcb_get_window_attributes_cookie_t ac = xcb_get_window_attributes_unchecked(c, w);
    xcb_get_window_attributes_reply_t *atts = xcb_get_window_attributes_reply(c, ac, NULL);

    xcb_get_geometry_cookie_t gc = xcb_get_geometry_unchecked(c, w);
    xcb_get_geometry_reply_t *geom = xcb_get_geometry_reply(c, gc, NULL);

    if ( atts && geom &&
            atts->map_state == XCB_MAP_STATE_VIEWABLE &&
            geom->width >= minSize && geom->height >= minSize ) {
        int x = 0, y = 0;
        if ( depth ) {
            x = geom->x + rx;
            y = geom->y + ry;
        }

        QRect r( x, y, geom->width, geom->height );
        if (!windows.contains(r))
            windows.append(r);

        xcb_query_tree_cookie_t tc = xcb_query_tree_unchecked(c, w);
        xcb_query_tree_reply_t *tree = xcb_query_tree_reply(c, tc, NULL);

        if (tree) {
            xcb_window_t* child = xcb_query_tree_children(tree);
            for (unsigned int i=0;i<tree->children_len;i++)
                getWindowsRecursive(windows, child[i], x, y, depth +1);
            free(tree);
        }
    }
    if (atts != NULL) free(atts);
    if (geom != NULL) free(geom);

    if ( depth == 0 )
        qSort(windows.begin(), windows.end(), lessThan);
}

xcb_window_t findRealWindow( xcb_window_t w, int depth )
{
    static char wm_state_s[] = "WM_STATE";

    xcb_connection_t* c = QX11Info::connection();

    if( depth > 5 ) {
        return 0;
    }

    xcb_intern_atom_cookie_t ac = xcb_intern_atom(c, 0, strlen(wm_state_s), wm_state_s);
    xcb_intern_atom_reply_t* wm_state = xcb_intern_atom_reply(c, ac, NULL);

    if (!wm_state) {
        qWarning("Unable to allocate xcb atom");
        return 0;
    }

    xcb_get_property_cookie_t pc = xcb_get_property(c, 0, w, wm_state->atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 0 );
    xcb_get_property_reply_t* pr = xcb_get_property_reply(c, pc, NULL);

    if (pr && pr->type != XCB_NONE) {
        free(pr);
        return w;
    }
    free(pr);
    free(wm_state);

    xcb_window_t ret = XCB_NONE;

    xcb_query_tree_cookie_t tc = xcb_query_tree_unchecked(c, w);
    xcb_query_tree_reply_t *tree = xcb_query_tree_reply(c, tc, NULL);

    if (tree) {
        xcb_window_t* child = xcb_query_tree_children(tree);
        for (unsigned int i=0;i<tree->children_len;i++)
            ret = findRealWindow(child[i], depth +1 );
        free(tree);
    }

    return ret;
}

xcb_window_t windowUnderCursor( bool includeDecorations )
{
    xcb_connection_t* c = QX11Info::connection();

    xcb_window_t child = QX11Info::appRootWindow();

    xcb_query_pointer_cookie_t pc = xcb_query_pointer(c,QX11Info::appRootWindow());
    xcb_query_pointer_reply_t *pr = xcb_query_pointer_reply(c, pc, NULL);

    if (pr && pr->child!=XCB_NONE )
        child = pr->child;

    free(pr);

    if( !includeDecorations ) {
        xcb_window_t real_child = findRealWindow( child );

        if( real_child != XCB_NONE ) { // test just in case
            child = real_child;
        }
    }

    return child;
}
