/* Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
**
** This file is part of BrickStock.
** BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
** by Robert Griebl, Copyright (C) 2004-2008.
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
#ifndef __CSELECTCOLOR_H__
#define __CSELECTCOLOR_H__

#include <qdialog.h>
//Added by qt3to4:
#include <QShowEvent>

#include "bricklink.h"

class CListView;


class CSelectColor : public QWidget {
	Q_OBJECT
public:
	CSelectColor ( QWidget *parent, const char *name = 0, Qt::WFlags fl = 0 );
	
	void setColor ( const BrickLink::Color * );
	const BrickLink::Color *color ( ) const;

signals:
	void colorSelected ( const BrickLink::Color *, bool );

protected slots:
	void colorChanged ( );
	void colorConfirmed ( );
	void languageChange ( );

protected:
	virtual void enabledChange ( bool old );
	virtual void showEvent ( QShowEvent * );

protected:
	CListView *w_colors;

	friend class CSelectColorDialog;
};

class CSelectColorDialog : public QDialog {
	Q_OBJECT
public:
	CSelectColorDialog ( QWidget *parent = 0, const char *name = 0, bool modal = true, Qt::WFlags fl = 0 );

	void setColor ( const BrickLink::Color * );
	const BrickLink::Color *color ( ) const;

	virtual int exec ( const QRect &pos = QRect ( ));

private slots:
	void checkColor ( const BrickLink::Color *, bool );
	
private:
	CSelectColor *w_sc;

	QPushButton *w_ok;
	QPushButton *w_cancel;
};

#endif