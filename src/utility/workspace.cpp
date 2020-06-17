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
#include <cstdlib>

#include <QLayout>
#include <QStackedLayout>
#include <QTabBar>
#include <QTabWidget>
#include <QEvent>
#include <QAction>
#include <QMenu>
#include <QApplication>
#include <QDesktopWidget>
#include <QToolButton>
#include <QPixmap>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

#include "utility.h"
#include "workspace.h"


Q_DECLARE_METATYPE(QWidget *)

/* XPM */
static const char * const tablist_xpm[] = {
"12 12 2 1",
"  c None",
"# c #000000",
"            ",
#if !defined(Q_OS_MAC)
"  ########  ",
#else
"############",
#endif
" ########## ",
" ########## ",
" ########## ",
" ########## ",
" ########## ",
#if !defined(Q_OS_MAC)
"############",
#else
"  ########  ",
#endif
"            ",
"            ",
"            ",
"            "};


static QString cleanWindowTitle(QWidget *window)
{
    QString s = window->windowTitle();
    s.replace(QLatin1String("[*]"), QLatin1String(window->isWindowModified() ? "*" : ""));
    return s;
}


class WindowMenu : public QMenu {
    Q_OBJECT
public:
    WindowMenu(Workspace *ws, bool shortcut = false, QWidget *parent = 0)
        : QMenu(parent), m_ws(ws), m_shortcut(shortcut)
    {
        connect(this, &QMenu::aboutToShow,
                this, &WindowMenu::buildMenu);
        connect(this, &QMenu::triggered,
                this, &WindowMenu::activateWindow);

        setEnabled(false);
    }

public slots:
    void checkEnabledStatus(int tabCount)
    {
        setEnabled(tabCount > 1);
    }

private slots:
    void buildMenu()
    {
        clear();
        QWidget *active = m_ws->activeWindow();

        int i = 0;
        foreach (QWidget *w, m_ws->windowList()) {
            QString s = cleanWindowTitle(w);
            if (m_shortcut && i < 10)
                s.prepend(QString("&%1   ").arg((i+1)%10));

            QAction *a = addAction(s);
            a->setCheckable(true);
            QVariant v;
            v.setValue(w);
            a->setData(v);
            a->setChecked(w == active);
            i++;
        }
    }

    void activateWindow(QAction *a)
    {
        if (a) {
            if (QWidget *w = qvariant_cast<QWidget *>(a->data()))
                m_ws->setActiveWindow(w);
        }
    }

private:
    Workspace *m_ws;
    bool       m_shortcut;
};


class TabBarSide : public QWidget {
    Q_OBJECT
public:
    TabBarSide(QTabBar *tabbar, QWidget *parent = 0)
        : QWidget(parent), m_tabbar(tabbar)
    { }

protected:
    void paintEvent(QPaintEvent *)
    {
        QPainter p(this);
        QStyleOptionTabBarBase option;
        option.initFrom(this);
        option.tabBarRect = option.rect;
        if (m_tabbar) {
            option.shape = m_tabbar->shape();
            option.documentMode = m_tabbar->documentMode();

            QStyleOptionTab tabOverlap;
            tabOverlap.shape = m_tabbar->shape();
            int overlap = m_tabbar->style()->pixelMetric(QStyle::PM_TabBarBaseOverlap, &tabOverlap, m_tabbar);
            option.rect.setTop(option.rect.bottom() - overlap + 1);
            option.rect.setHeight(overlap);
        }
        style()->drawPrimitive(QStyle::PE_FrameTabBarBase, &option, &p, this);
    }

private:
    QTabBar *m_tabbar;
};

class TabBar : public QTabBar {
    Q_OBJECT
public:
    TabBar(QWidget *parent = 0)
        : QTabBar(parent)
    {
        setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    }

signals:
    void tabCountChanged(int);

protected:
    void tabInserted(int index)
    {
        QTabBar::tabInserted(index);
        emit tabCountChanged(count());
    }

    void tabRemoved(int index)
    {
        QTabBar::tabRemoved(index);
        emit tabCountChanged(count());
    }
};

class TabBarSideButton : public QToolButton {
    Q_OBJECT

public:
    TabBarSideButton(QTabBar *tabbar, QWidget *parent = 0)
        : QToolButton(parent), m_tabbar(tabbar)
    {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    }

    QSize sizeHint() const
    {
        QSize sb = QToolButton::sizeHint();
        QSize st = m_tabbar->sizeHint();
        return QSize(sb.width(), st.height());
    }

#ifdef Q_OS_MAC
    void setIcon(const QIcon &icon)
    {
        QPixmap pnormal = icon.pixmap(iconSize());
        QPixmap pactive = pnormal;
        QColor textColor = palette().color(QPalette::Text);
        {
            QPainter p(&pnormal);
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(pnormal.rect(), textColor);
        }
        {
            QPainter p(&pactive);
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(pactive.rect(), Utility::contrastColor(textColor, 0.2f));
        }
        QIcon icon2(pnormal);
        icon2.addPixmap(pactive, QIcon::Active, QIcon::Off);
        icon2.addPixmap(pactive, QIcon::Active, QIcon::On);
        icon2.addPixmap(pactive, QIcon::Normal, QIcon::On);
        QToolButton::setIcon(icon2);
    }

protected:
    void paintEvent(QPaintEvent *)
    {
        QPainter p(this);
        QIcon::Mode mode = isEnabled() ? (underMouse() || isDown() ? QIcon::Active : QIcon::Normal ) : QIcon::Disabled;
        QIcon::State state = isDown() ? QIcon::On : QIcon::Off;

        icon().paint(&p, rect(), Qt::AlignCenter, mode, state);
    }
#endif

private:
    QTabBar *m_tabbar;
};


Workspace::Workspace(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{

    m_tabbar = new TabBar(this);
    m_tabbar->setElideMode(Qt::ElideMiddle);
    m_tabbar->setDocumentMode(true);
    m_tabbar->setUsesScrollButtons(true);
    m_tabbar->setMovable(true);
    m_tabbar->setTabsClosable(true);


    m_left = new TabBarSide(m_tabbar);
    m_right = new TabBarSide(m_tabbar);
    m_stack = new QStackedLayout();

    TabBarSideButton *tabList = new TabBarSideButton(m_tabbar);
    tabList->setIcon(QIcon(QPixmap(tablist_xpm)));
    tabList->setAutoRaise(true);
    tabList->setPopupMode(QToolButton::InstantPopup);
    tabList->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tabList->setToolTip(tr("Show a list of all open documents"));
    tabList->setMenu(windowMenu(false, this));

    QHBoxLayout *rightlay = new QHBoxLayout(m_right);
    rightlay->setMargin(0);
    rightlay->addWidget(tabList);

    QHBoxLayout *tabbox = new QHBoxLayout();
    tabbox->setMargin(0);
    tabbox->setSpacing(0);
    tabbox->addWidget(m_left);
    tabbox->addWidget(m_tabbar, 10);
    tabbox->addWidget(m_right);
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addLayout(tabbox);
    layout->addLayout(m_stack, 10);
    setLayout(layout);

    connect(m_tabbar, &QTabBar::currentChanged,
            this, &Workspace::activateTab);
    connect(m_tabbar, &QTabBar::tabCloseRequested,
            this, &Workspace::closeTab);
    connect(m_tabbar, &TabBar::tabCountChanged,
            this, &Workspace::updateVisibility);
    connect(m_tabbar, &QTabBar::tabMoved,
            this, &Workspace::moveTab);
    connect(m_stack, &QStackedLayout::widgetRemoved,
            this, &Workspace::removeTab);

    updateVisibility();
}

QMenu *Workspace::windowMenu(bool hasShortcut, QWidget *parent)
{
    WindowMenu *m = new WindowMenu(this, hasShortcut, parent);
    connect(m_tabbar, &TabBar::tabCountChanged,
            m, &WindowMenu::checkEnabledStatus);
    return m;
}

void Workspace::updateVisibility()
{
    bool b = (m_tabbar->count() > 1);

    // showing/hiding a tabbar in document mode conflicts with the border
    // metric changes in QTabBarPrivate::updateMacBorderMetrics()
    m_tabbar->setMaximumHeight(b ? QWIDGETSIZE_MAX : 0);
    m_left->setMaximumHeight(b ? QWIDGETSIZE_MAX : 0);
    m_right->setMaximumHeight(b ? QWIDGETSIZE_MAX : 0);
}

void Workspace::addWindow(QWidget *w)
{
    if (!w)
        return;

    int idx = m_stack->addWidget(w);
    m_tabbar->insertTab(idx, cleanWindowTitle(w));
    if (!w->windowIcon().isNull())
        m_tabbar->setTabIcon(idx, w->windowIcon());

    w->installEventFilter(this);
}

void Workspace::removeTab(int idx)
{
    m_tabbar->removeTab(idx);
}

void Workspace::moveTab(int from, int to)
{
    m_stack->blockSignals(true);
    QWidget *w = m_stack->widget(from);
    m_stack->removeWidget(w);
    m_stack->insertWidget(to, w);
    m_stack->blockSignals(false);
}

void Workspace::activateTab(int idx)
{
    m_stack->setCurrentIndex(idx);
    emit windowActivated(m_stack->widget(idx));
}

void Workspace::setActiveWindow(QWidget *w)
{
    m_tabbar->setCurrentIndex(m_stack->indexOf(w));
}

QList<QWidget *> Workspace::windowList() const
{
    QList<QWidget *> res;

    for (int i = 0; i < m_stack->count(); i++)
        res << m_stack->widget(i);

    return res;
}

QWidget *Workspace::activeWindow() const
{
    return m_stack->currentWidget();
}

bool Workspace::eventFilter(QObject *o, QEvent *e)
{
    if (QWidget *w = qobject_cast<QWidget *>(o)) {
        switch (e->type()) {
        case QEvent::WindowTitleChange:
        case QEvent::ModifiedChange:
            m_tabbar->setTabText(m_stack->indexOf(w), cleanWindowTitle(w));
            break;
        case QEvent::WindowIconChange:
            m_tabbar->setTabIcon(m_stack->indexOf(w), w->windowIcon());
            break;
        default:
            break;
        }
    }

    return QWidget::eventFilter(o, e);
}

void Workspace::closeTab(int idx)
{
    m_stack->widget(idx)->close();
}

#include "workspace.moc"