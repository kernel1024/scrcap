#include "qxtglobalshortcut.h"
/****************************************************************************
** Copyright (c) 2006 - 2011, the LibQxt project.
** See the Qxt AUTHORS file for a list of authors and copyright holders.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of the LibQxt project nor the
**       names of its contributors may be used to endorse or promote products
**       derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
** <http://libqxt.org>  <foundation@libqxt.org>
*****************************************************************************/

#include <QApplication>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>
#include <QVector>
#include <QX11Info>

xcb_keysym_t qtKeyToKeysym(const Qt::Key key, const Qt::KeyboardModifiers mods)
{
    unsigned int code;
    switch( key )
    {
        // modifiers are handled separately
        case Qt::Key_Shift: code = XK_Shift_L; break;
        case Qt::Key_Control: code = XK_Control_L; break;
        case Qt::Key_Meta: code = XK_Meta_L; break;
        case Qt::Key_Alt: code = XK_Alt_L; break;
        case Qt::Key_Escape: code = XK_Escape; break;
        case Qt::Key_Tab: code = XK_Tab; break;
        case Qt::Key_Backtab: code = XK_Tab; break;
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
        default: code = 0; break;
    }

    if( key >= Qt::Key_F1 && key <= Qt::Key_F35 )
    {
        code = XK_F1 + key - Qt::Key_F1;
    }

    if ( code == 0 ) {
        QKeySequence seq(key);
        if ((mods & Qt::ControlModifier) && seq.toString().length() == 1) {
            QString s = seq.toString();
              if ((mods & Qt::ShiftModifier) == 0)
                    s = s.toLower();

              code = s.utf16()[0];
        } else if (seq.toString().length() == 1){
              code = seq.toString().utf16()[0];
        }
    }

    return code;
}

bool ungrabKey(xcb_keycode_t keycode, uint16_t modifiers, xcb_window_t window)
{
    xcb_connection_t* c = QX11Info::connection();

    xcb_void_cookie_t vc = xcb_ungrab_key(c, keycode, window, modifiers);

    return (xcb_request_check(c, vc)==NULL);
}

bool grabKey(xcb_keycode_t keycode, uint16_t modifiers, xcb_window_t window)
{
    xcb_connection_t* c = QX11Info::connection();

    xcb_void_cookie_t vc = xcb_grab_key(c, true, window, modifiers, keycode,
                                        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

    if (xcb_request_check(c, vc)) {
        ungrabKey(keycode, modifiers, window);
        return false;
    }

    return true;
}

bool QxtGlobalShortcut::nativeEventFilter(const QByteArray & eventType,
    void *message, long *result)
{
    Q_UNUSED(result);

    xcb_key_press_event_t *kev = 0;
    if (eventType == "xcb_generic_event_t") {
        xcb_generic_event_t *ev = static_cast<xcb_generic_event_t *>(message);
        if ((ev->response_type & 127) == XCB_KEY_PRESS)
            kev = static_cast<xcb_key_press_event_t *>(message);
    }

    if (kev != 0) {
        unsigned int keycode = kev->detail;
        unsigned int keystate = kev->state & (XCB_MOD_MASK_1 | XCB_MOD_MASK_CONTROL |
                                              XCB_MOD_MASK_4 | XCB_MOD_MASK_SHIFT);
        // Mod1Mask == Alt, Mod4Mask == Meta
        activateShortcut(keycode, keystate);
    }
    return false;
}

uint16_t QxtGlobalShortcut::nativeModifiers(Qt::KeyboardModifiers modifiers)
{
    // ShiftMask, LockMask, ControlMask, Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, and Mod5Mask
    uint16_t native = 0;
    if (modifiers & Qt::ShiftModifier)
        native |= XCB_MOD_MASK_SHIFT;
    if (modifiers & Qt::ControlModifier)
        native |= XCB_MOD_MASK_CONTROL;
    if (modifiers & Qt::AltModifier)
        native |= XCB_MOD_MASK_1;
    if (modifiers & Qt::MetaModifier)
        native |= XCB_MOD_MASK_4;

    return native;
}

xcb_keycode_t QxtGlobalShortcut::nativeKeycode(Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    xcb_connection_t* c = QX11Info::connection();

    xcb_keysym_t sym = qtKeyToKeysym(key, modifiers);

    xcb_key_symbols_t *syms = xcb_key_symbols_alloc(c);

    xcb_keycode_t *keyCodes = xcb_key_symbols_get_keycode(syms, sym);
    const xcb_keycode_t ret = keyCodes[0];
    free(keyCodes);
    xcb_key_symbols_free(syms);

    return ret;
}

bool QxtGlobalShortcut::registerShortcut(xcb_keycode_t nativeKey, uint16_t nativeMods)
{
    return grabKey(nativeKey, nativeMods, QX11Info::appRootWindow());
}

bool QxtGlobalShortcut::unregisterShortcut(xcb_keycode_t nativeKey, uint16_t nativeMods)
{
    return ungrabKey(nativeKey, nativeMods, QX11Info::appRootWindow());
}
