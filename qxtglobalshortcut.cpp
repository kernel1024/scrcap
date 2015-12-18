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

#include <QAbstractEventDispatcher>
#include <QtDebug>
#include <xcb/xcb.h>

int QxtGlobalShortcut::ref = 0;
QHash<QPair<xcb_keycode_t, uint16_t>, QxtGlobalShortcut*> QxtGlobalShortcut::shortcuts;

/*
    Example usage:

    QxtGlobalShortcut* shortcut = new QxtGlobalShortcut(window);
    connect(shortcut, SIGNAL(activated()), window, SLOT(toggleVisibility()));
    shortcut->setShortcut(QKeySequence("Ctrl+Shift+F12"));
*/

QxtGlobalShortcut::QxtGlobalShortcut(QObject* parent)
        : QObject(parent), m_enabled(false), key(Qt::Key(0)), mods(Qt::NoModifier)
{
    if (ref == 0) {
        QAbstractEventDispatcher::instance()->installNativeEventFilter(this);
    }
    ++ref;
}

QxtGlobalShortcut::QxtGlobalShortcut(const QKeySequence& shortcut, QObject* parent)
        : QObject(parent), m_enabled(true), key(Qt::Key(0)), mods(Qt::NoModifier)
{
    if (ref == 0) {
        QAbstractEventDispatcher::instance()->installNativeEventFilter(this);
    }
    ++ref;

    setShortcut(shortcut);
}

QxtGlobalShortcut::~QxtGlobalShortcut()
{
    if (key != 0)
        unsetShortcut();

    --ref;
    if (ref == 0) {
        QAbstractEventDispatcher *ed = QAbstractEventDispatcher::instance();
        if (ed != 0) {
            ed->removeNativeEventFilter(this);
        }
    }
}

bool QxtGlobalShortcut::setShortcut(const QKeySequence& shortcut)
{
    if (key != 0)
        unsetShortcut();

    Qt::KeyboardModifiers allMods = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;

    key = Qt::Key(0);
    if (!shortcut.isEmpty())
        key = Qt::Key((shortcut[0] ^ allMods) & shortcut[0]);

    mods = Qt::KeyboardModifiers(0);
    if (!shortcut.isEmpty())
        mods = Qt::KeyboardModifiers(shortcut[0] & allMods);

    const xcb_keycode_t nativeKey = nativeKeycode(key, mods);
    const uint16_t nativeMods = nativeModifiers(mods);
    const bool res = registerShortcut(nativeKey, nativeMods);
    if (res)
        shortcuts.insert(qMakePair(nativeKey, nativeMods), this);
    else
        qWarning() << "QxtGlobalShortcut failed to register:" << QKeySequence(key + mods).toString();
    return res;
}

bool QxtGlobalShortcut::unsetShortcut()
{
    bool res = false;
    const xcb_keycode_t nativeKey = nativeKeycode(key, mods);
    const uint16_t nativeMods = nativeModifiers(mods);
    if (shortcuts.value(qMakePair(nativeKey, nativeMods)) == this)
        res = unregisterShortcut(nativeKey, nativeMods);
    if (res)
        shortcuts.remove(qMakePair(nativeKey, nativeMods));
    else
        qWarning() << "QxtGlobalShortcut failed to unregister:" << QKeySequence(key + mods).toString();
    key = Qt::Key(0);
    mods = Qt::KeyboardModifiers(0);
    return res;
}

void QxtGlobalShortcut::activateShortcut(xcb_keycode_t nativeKey, uint16_t nativeMods)
{
    QxtGlobalShortcut* shortcut = shortcuts.value(qMakePair(nativeKey, nativeMods));
    if (shortcut && shortcut->isEnabled())
        emit shortcut->activated();
}

QKeySequence QxtGlobalShortcut::shortcut() const
{
    return QKeySequence(key | mods);
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
