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

#include <QPointer>
#include <QMutexLocker>
#include <QApplication>
#include <QDebug>
#include <xcb/xcb.h>

#include "qxtglobalshortcut.h"
#include "xcbtools.h"

/*
    Example usage:

    QxtGlobalShortcut* shortcut = new QxtGlobalShortcut(QKeySequence("Ctrl+Shift+F12"), window);
    connect(shortcut, &QxtGlobalShortcut::activated, window, &QWidget::toggleVisibility);
*/

QxtGlobalShortcut::QxtGlobalShortcut(const QKeySequence& shortcut, QObject* parent)
        : QObject(parent)
{
    if (!shortcut.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_key = shortcut[0].key();
        m_mods = shortcut[0].keyboardModifiers();
#else
        Qt::KeyboardModifiers allMods = Qt::ShiftModifier | Qt::ControlModifier
                                        | Qt::AltModifier | Qt::MetaModifier;

        auto sc = static_cast<unsigned int>(shortcut[0]);
        m_key = static_cast<Qt::Key>((sc ^ allMods) & sc);
        m_mods = Qt::KeyboardModifiers(sc & allMods);
#endif

        m_enabled = true;
        QxtGlobalShortcutFilter::setupShortcut(this);
    }
}

QxtGlobalShortcut::~QxtGlobalShortcut()
{
    Q_EMIT destroying(this);
}

QKeySequence QxtGlobalShortcut::getShortcut() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QKeySequence seq(QKeyCombination(m_mods,m_key));
#else
    QKeySequence seq(static_cast<int>(m_key | m_mods));
#endif
    return seq;
}

bool QxtGlobalShortcut::isEnabled() const
{
    return m_enabled;
}

void QxtGlobalShortcut::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void QxtGlobalShortcut::setDisabled(bool disabled)
{
    m_enabled = !disabled;
}


// ---------------- QxtGlobalShortcutFilter ------------

QxtGlobalShortcutFilter::QxtGlobalShortcutFilter(QObject *parent)
    : ZAbstractXCBEventListener(parent)
{
    ZXCBTools::addEventListener(this,XCB_KEY_PRESS);
}

QxtGlobalShortcutFilter::~QxtGlobalShortcutFilter()
{
    ZXCBTools::removeEventListener(this);
}

bool QxtGlobalShortcutFilter::addShortcut(QxtGlobalShortcut *shortcut)
{
    if (shortcut == nullptr)
        return false;
    if (m_shortcuts.contains(shortcut))
        return true;

    const xcb_keycode_t nativeKey = ZXCBTools::nativeKeycode(shortcut->m_key, shortcut->m_mods);
    const uint16_t nativeMods = ZXCBTools::nativeModifiers(shortcut->m_mods);
    const bool res = ZXCBTools::registerShortcut(nativeKey, nativeMods);
    if (res) {
        m_shortcuts.append(shortcut);
    } else {
        qWarning() << "QxtGlobalShortcut failed to register";
    }
    return res;
}

bool QxtGlobalShortcutFilter::removeShortcut(QxtGlobalShortcut *shortcut)
{
    Q_ASSERT(shortcut!=nullptr);

    bool res = false;
    if (!m_shortcuts.contains(shortcut)) return res;

    const xcb_keycode_t nativeKey = ZXCBTools::nativeKeycode(shortcut->m_key, shortcut->m_mods);
    const uint16_t nativeMods = ZXCBTools::nativeModifiers(shortcut->m_mods);
    res = ZXCBTools::unregisterShortcut(nativeKey, nativeMods);
    if (res) {
        m_shortcuts.removeAll(shortcut);
    } else {
        qWarning() << "QxtGlobalShortcut failed to unregister";
    }
    return res;
}

void QxtGlobalShortcutFilter::activateShortcut(xcb_keycode_t nativeKey, uint16_t nativeMods)
{
    for (const auto* obj : qAsConst(m_shortcuts)) {
        auto* sc = const_cast<QxtGlobalShortcut *>(qobject_cast<const QxtGlobalShortcut *>(obj));
        if (sc && sc->isEnabled()) {
            const xcb_keycode_t nativeKeySc = ZXCBTools::nativeKeycode(sc->m_key, sc->m_mods);
            const uint16_t nativeModsSc = ZXCBTools::nativeModifiers(sc->m_mods);
            if ((nativeKey == nativeKeySc) &&
                    (nativeMods == nativeModsSc)) {
                QMetaObject::invokeMethod(sc,[sc](){
                    Q_EMIT sc->activated();
                },Qt::QueuedConnection);
            }
        }
    }
}

bool QxtGlobalShortcutFilter::setupShortcut(QxtGlobalShortcut *shortcut)
{
    static QPointer<QxtGlobalShortcutFilter> inst;
    static QMutex scMutex;

    QMutexLocker locker(&scMutex);

    if (inst.isNull()) {
        inst = new QxtGlobalShortcutFilter(QCoreApplication::instance());
    }

    bool res = inst->addShortcut(shortcut);
    if (res) {
        QObject::connect(shortcut,&QxtGlobalShortcut::destroying,[](QxtGlobalShortcut* sc){
            QMutexLocker locker(&scMutex);

            if (inst.isNull()) return;
            inst->removeShortcut(sc);
            if (inst->m_shortcuts.isEmpty()) {
                inst->deleteLater();
            }
        });
    }

    return res;
}

void QxtGlobalShortcutFilter::nativeEventHandler(const xcb_generic_event_t *event)
{
    const auto *kev = reinterpret_cast<const xcb_key_press_event_t *>(event);

    if (kev != nullptr) {
        xcb_keycode_t keycode = kev->detail;
        const unsigned short modifiersMask = (XCB_MOD_MASK_1 | XCB_MOD_MASK_CONTROL | // NOLINT
                                              XCB_MOD_MASK_4 | XCB_MOD_MASK_SHIFT); // NOLINT
        uint16_t keystate = kev->state & modifiersMask;
        // Mod1Mask == Alt, Mod4Mask == Meta
        activateShortcut(keycode, keystate);
    }
}
