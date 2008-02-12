/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __CFILTEREDIT_H__
#define __CFILTEREDIT_H__

#include <QLineEdit>

class QMenu;
class QToolButton;

class CFilterEdit : public QLineEdit {
    Q_OBJECT
public:
    CFilterEdit(QWidget *parent = 0);
    void setMenu(QMenu *menu);
    QMenu *menu() const;

    void setMenuIcon(const QIcon &icon);
    void setClearIcon(const QIcon &icon);

protected:
    virtual void resizeEvent(QResizeEvent *);

protected slots:
    void updateClearButton(const QString &);

private:
    void doLayout(bool setpositions);

private:
    QToolButton *w_menu;
    QToolButton *w_clear;
};

#endif
