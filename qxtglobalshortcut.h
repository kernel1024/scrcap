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

#ifndef QXTGLOBALSHORTCUT_H
#define QXTGLOBALSHORTCUT_H

#include <QObject>
#include <QKeySequence>
#include <xcb/xcb.h>
#include "xcbtools.h"

class QxtGlobalShortcutFilter;

class QxtGlobalShortcut : public QObject
{
    Q_OBJECT

    friend class QxtGlobalShortcutFilter;

public:
    explicit QxtGlobalShortcut(const QKeySequence& shortcut, QObject* parent = nullptr);
    ~QxtGlobalShortcut() override;

    QKeySequence getShortcut() const;
    bool isEnabled() const;

public Q_SLOTS:
    void setEnabled(bool enabled = true);
    void setDisabled(bool disabled = true);

Q_SIGNALS:
    void activated();
    void destroying(QxtGlobalShortcut* shortcut);

private:
    Q_DISABLE_COPY(QxtGlobalShortcut)

    Qt::Key m_key { Qt::Key(0) };
    Qt::KeyboardModifiers m_mods { Qt::NoModifier };
    bool m_enabled { false };
};

using QxtNativeShortcut = QPair<xcb_keycode_t, uint16_t>;

class QxtGlobalShortcutFilter : public ZAbstractXCBEventListener
{
    Q_OBJECT
private:
    Q_DISABLE_COPY(QxtGlobalShortcutFilter)

    QObjectList m_shortcuts;

    bool addShortcut(QxtGlobalShortcut* shortcut);
    bool removeShortcut(QxtGlobalShortcut* shortcut);
    void activateShortcut(xcb_keycode_t nativeKey, uint16_t nativeMods);

public:
    QxtGlobalShortcutFilter(QObject* parent = nullptr);
    ~QxtGlobalShortcutFilter() override;
    static bool setupShortcut(QxtGlobalShortcut* shortcut);

protected:
    void nativeEventHandler(const xcb_generic_event_t* event) override;

};

#endif // QXTGLOBALSHORTCUT_H
