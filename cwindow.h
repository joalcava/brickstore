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
#ifndef __CWINDOW_H__
#define __CWINDOW_H__

#include <QWidget>
#include <QRegExp>
#include <QHash>
#include <qdom.h>

//#include "citemview.h"
#include "bricklink.h"
#include "cdocument.h"
#include "cconfig.h"
#include "cmoney.h"

class QToolButton;
class QComboBox;
//class QListViewItem;
//class CItemView;
class QTableView;
class CFrameWork;
class CUndoStack;
class CFilterEdit;


class CWindow : public QWidget, public IDocumentView {
    Q_OBJECT

public:
    CWindow(CDocument *doc, QWidget *parent = 0);
    ~CWindow();

    CDocument *document() { return m_doc; }

    enum Filter {
        All        = -1,

        Prices     = -2,
        Texts      = -3,
        Quantities = -4,

        FilterCountSpecial = 4
    };

    enum MergeFlags {
        MergeAction_None         = 0x00000000,
        MergeAction_Force        = 0x00000001,
        MergeAction_Ask          = 0x00000002,
        MergeAction_Mask         = 0x0000ffff,

        MergeKeep_Old            = 0x00000000,
        MergeKeep_New            = 0x00010000,
        MergeKeep_Mask           = 0xffff0000
    };

    CDocument::ItemList sortedItems(bool selection_only = false) const;

    uint setItems(const BrickLink::InvItemList &items, int multiply = 1);
    uint addItems(const BrickLink::InvItemList &items, int multiply = 1, uint mergeflags = MergeAction_None, bool dont_change_sorting = false);
    void deleteItems(const BrickLink::InvItemList &items);

    void mergeItems(const CDocument::ItemList &items, int globalmergeflags = MergeAction_Ask);

    void subtractItems(const BrickLink::InvItemList &items);
    void copyRemarks(const BrickLink::InvItemList &items);

// bool hasFilter ( ) const;
// void setFilter ( const QRegExp &exp, Field f );
// void setFilterExpression ( const QRegExp &exp );
// void setFilterField ( Field f );
// QRegExp filterExpression ( ) const;
// Field filterField ( ) const;

// InvItemList &selectedItems ( );
// void setSelectedItems ( const InvItemList &items );

    virtual QDomElement createGuiStateXML(QDomDocument doc);
    virtual bool parseGuiStateXML(QDomElement root);

    bool isDifferenceMode() const;

public slots:
    void on_view_difference_mode_toggled(bool);

    void on_file_save_triggered();
    void on_file_saveas_triggered();
    void on_file_export_bl_xml_triggered();
    void on_file_export_bl_xml_clip_triggered();
    void on_file_export_bl_update_clip_triggered();
    void on_file_export_bl_invreq_clip_triggered();
    void on_file_export_bl_wantedlist_clip_triggered();
    void on_file_export_briktrak_triggered();

    void on_file_print_triggered();
    void on_file_close_triggered();

    void on_edit_cut_triggered();
    void on_edit_copy_triggered();
    void on_edit_paste_triggered();
    void on_edit_delete_triggered();

    void on_edit_subtractitems_triggered();
    void on_edit_copyremarks_triggered();
    void on_edit_mergeitems_triggered();
    void on_edit_partoutitems_triggered();

    void on_edit_reset_diffs_triggered();

    void on_edit_select_all_triggered();
    void on_edit_select_none_triggered();

    void on_edit_status_include_triggered();
    void on_edit_status_exclude_triggered();
    void on_edit_status_extra_triggered();
    void on_edit_status_toggle_triggered();
    void on_edit_cond_new_triggered();
    void on_edit_cond_used_triggered();
    void on_edit_cond_toggle_triggered();
    void on_edit_color_triggered();
    void on_edit_qty_multiply_triggered();
    void on_edit_qty_divide_triggered();
    void on_edit_price_set_triggered();
    void on_edit_price_round_triggered();
    void on_edit_price_to_priceguide_triggered();
    void on_edit_price_inc_dec_triggered();
    void on_edit_bulk_triggered();
    void on_edit_sale_triggered();
    void on_edit_retain_yes_triggered();
    void on_edit_retain_no_triggered();
    void on_edit_retain_toggle_triggered();
    void on_edit_stockroom_yes_triggered();
    void on_edit_stockroom_no_triggered();
    void on_edit_stockroom_toggle_triggered();
    void on_edit_reserved_triggered();
    void on_edit_remark_add_triggered();
    void on_edit_remark_rem_triggered();
    void on_edit_remark_set_triggered();
    void on_edit_comment_add_triggered();
    void on_edit_comment_rem_triggered();
    void on_edit_comment_set_triggered();

    void on_edit_bl_catalog_triggered();
    void on_edit_bl_priceguide_triggered();
    void on_edit_bl_lotsforsale_triggered();
    void on_edit_bl_myinventory_triggered();

    void on_view_save_default_col_triggered();

    void addItem(BrickLink::InvItem *, uint);

    void setPrice(money_t);

signals:
    void selectionChanged(const BrickLink::InvItemList &);
    void statisticsChanged();

protected:
    virtual void closeEvent(QCloseEvent *e);
    virtual void changeEvent(QEvent *e);

private slots:
    void applyFilter();
    void applyFilterField(QAction *);
    void updateCaption();

    void contextMenu(const QPoint &);
    void priceGuideUpdated(BrickLink::PriceGuide *);
    void updateErrorMask();

    void itemsAddedToDocument(const CDocument::ItemList &);
    void itemsRemovedFromDocument(const CDocument::ItemList &);
    void itemsChangedInDocument(const CDocument::ItemList &, bool);

private:
    CDocument::ItemList exportCheck() const;

private:
    CDocument * m_doc;
    QRegExp        m_filter_expression;
    int            m_filter_field;

// bool m_ignore_selection_update;

// QHash<int, CItemViewItem *>  m_lvitems;

    //CFilterEdit *  w_filter;
    QTableView *   w_list;

    uint                           m_settopg_failcnt;
    QMultiHash<BrickLink::PriceGuide *, CDocument::Item *> *m_settopg_list;
    BrickLink::Time    m_settopg_time;
    BrickLink::Price   m_settopg_price;
};

#endif
