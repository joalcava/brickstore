/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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

#include <QObject>

class ProgressDialog;


class UpdateDatabase : public QObject
{
    Q_OBJECT
public:
    UpdateDatabase(ProgressDialog *pd);

private slots:
    void gotten();

private:
    QString decompress(const QString &src, const QString &dst);

private:
    ProgressDialog *m_progress;
};