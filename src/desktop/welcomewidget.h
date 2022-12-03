/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
**
** This file is part of BrickStore.
**
** This file may be distributed and/or modified under the terms of the GNU
** General Public License version 2 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/
#pragma once

#include <QWidget>
#include <QPointer>
#include <QIcon>

QT_FORWARD_DECLARE_CLASS(QGroupBox)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QGraphicsOpacityEffect)
QT_FORWARD_DECLARE_CLASS(QPropertyAnimation)

class BetterCommandButton;


class WelcomeWidget : public QWidget
{
    Q_OBJECT

public:
    WelcomeWidget(QWidget *parent = nullptr);

    void fadeIn();
    void fadeOut();

protected:
    void changeEvent(QEvent *e) override;

private:
    void updateVersionsText();

    void languageChange();

private:
    void fade(bool in);

    QGroupBox *m_recent_frame;
    QGroupBox *m_document_frame;
    QGroupBox *m_import_frame;
    QPointer<QLabel> m_no_recent;
    QLabel *m_versions;
    QIcon m_docIcon;
    QGraphicsOpacityEffect *m_effect;
    QPropertyAnimation *m_animation;
};
