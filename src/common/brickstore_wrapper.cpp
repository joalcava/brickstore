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

#include <QQmlEngine>
#include <QQmlContext>
#include <QFile>
#include <QUrl>
#include <QGuiApplication>
#include <QFontDatabase>
#include "qqmlinfo.h"

#include "utility/utility.h"
#include "common/currency.h"
#include "bricklink/picture.h"
#include "bricklink/priceguide.h"
#include "bricklink/order.h"
#include "common/actionmanager.h"
#include "common/application.h"
#include "common/document.h"
#include "common/documentmodel.h"
#include "common/documentio.h"
#include "common/recentfiles.h"
#include "brickstore_wrapper.h"
#include "version.h"


/*! \qmltype BrickStore
    \inherits QtObject
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This singleton represents global settings and state.

    A kitchen sink type singleton for all global state, settings and utility functions.
*/

/*! \qmlsignal BrickStore::showSettings(string page)
    \internal
*/

/*! \qmlproperty string BrickStore::defaultCurrencyCode
    \readonly
    The user's default ISO currency code (e.g. \c EUR).
*/
/*! \qmlproperty string BrickStore::versionNumber
    \readonly
    BrickStore's version as a string (e.g. \c "2021.10.2").
*/
/*! \qmlproperty string BrickStore::buildNumber
    \readonly
    BrickStore's build number as a string (e.g. \c "42").
*/
/*! \qmlproperty bool BrickStore::databaseValid
    \readonly
    Returns whether the current database is valid or not.
*/
/*! \qmlproperty date BrickStore::lastDatabaseUpdate
    \readonly
    This property holds the date and time of the last successful database update.
*/
/*! \qmlproperty bool BrickStore::online
    \readonly
    The current online state of the application. This is mirroring the operating system's online
    state.
*/
/*! \qmlproperty Document BrickStore::activeDocument
    \readonly
    The currently active document, i.e. the document the user is working on. Can be \c null, if
    no documents are open, but also if the quickstart page is active.
*/


QmlBrickStore::QmlBrickStore()
    : m_columnLayouts(new ColumnLayoutsModel(this))
{
    setObjectName(u"BrickStore"_qs);

    connect(Application::inst(), &Application::showSettings,
            this, &QmlBrickStore::showSettings);

    connect(ActionManager::inst(), &ActionManager::activeDocumentChanged,
            this, &QmlBrickStore::activeDocumentChanged);

    connect(Config::inst(), &Config::defaultCurrencyCodeChanged,
            this, &QmlBrickStore::defaultCurrencyCodeChanged);
}

DocumentList *QmlBrickStore::documents() const
{
    return DocumentList::inst();
}

Config *QmlBrickStore::config() const
{
    return Config::inst();
}

QString QmlBrickStore::versionNumber() const
{
    return QLatin1String(BRICKSTORE_VERSION);
}

QString QmlBrickStore::buildNumber() const
{
    return QLatin1String(BRICKSTORE_BUILD_NUMBER);
}

RecentFiles *QmlBrickStore::recentFiles() const
{
    return RecentFiles::inst();
}

ColumnLayoutsModel *QmlBrickStore::columnLayouts() const
{
    return m_columnLayouts;
}

QVariantMap QmlBrickStore::about() const
{
    return Application::inst()->about();
}

QmlDebug *QmlBrickStore::debug() const
{
    if (!m_debug)
        m_debug = new QmlDebug(const_cast<QmlBrickStore *>(this));
    return m_debug;
}

QString QmlBrickStore::defaultCurrencyCode() const
{
    return Config::inst()->defaultCurrencyCode();
}

/*! \qmlmethod string BrickStore::symbolForCurrencyCode(string currencyCode)

    Returns the currency symbol for the ISO \a currencyCode if available or the \a currencyCode
    itself otherwise. E.g. \c EUR will be mapped to \c €.
*/
QString QmlBrickStore::symbolForCurrencyCode(const QString &currencyCode) const
{
    static QHash<QString, QString> cache;
    QString s = cache.value(currencyCode);
    if (s.isEmpty()) {
         const auto allLoc = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript,
                                                      QLocale::AnyCountry);

         for (auto &loc : allLoc) {
             if (loc.currencySymbol(QLocale::CurrencyIsoCode) == currencyCode) {
                 s = loc.currencySymbol(QLocale::CurrencySymbol);
                 break;
             }
         }
         cache.insert(currencyCode, s.isEmpty() ? currencyCode : s);
    }
    return s;
}

/*! \qmlmethod string BrickStore::toCurrencyString(real value, string symbol = "", int precision = 3)

    Correctly formats the given currency \a value according to the user's locale and returns it as
    a string. Also appends a currency \a symbol if provided.
    The default number of decimal places is \c 3, but you can change this via the \a precision
    parameter.
*/
QString QmlBrickStore::toCurrencyString(double value, const QString &symbol, int precision) const
{
    return Currency::toDisplayString(value, symbol, precision);
}

/*! \qmlmethod string BrickStore::toWeightString(real value, bool showUnit = false)

    Correctly formats the given weight \a value according to the user's locale and the
    metric/imperial setting in BrickStore and returns it as a string. Also
    appends the corresponding unit if \a showUnit is set to \c true.
*/
QString QmlBrickStore::toWeightString(double value, bool showUnit) const
{
    return Utility::weightToString(value, Config::inst()->measurementSystem(), true, showUnit);
}


/*! \qmlmethod stringlist BrickStore::nameFiltersForBrickLinkXML(bool includeAll = false)

    Returns a list of file extension that can be used when creating a FileDialog to open
    BrickLink XML files. If \a includeAll is set, a match-everything filter \c * will be added to
    this list.
*/
QStringList QmlBrickStore::nameFiltersForBrickLinkXML(bool includeAll) const
{
    return DocumentIO::nameFiltersForBrickLinkXML(includeAll);
}

/*! \qmlmethod stringlist BrickStore::nameFiltersForBrickStoreXML(bool includeAll = false)

    Returns a list of file extension that can be used when creating a FileDialog to open
    BrickStore document files. If \a includeAll is set, a match-everything filter \c * will be
    added to this list.
*/
QStringList QmlBrickStore::nameFiltersForBrickStoreXML(bool includeAll) const
{
    return DocumentIO::nameFiltersForBrickStoreXML(includeAll);
}

/*! \qmlmethod stringlist BrickStore::nameFiltersForLDraw(bool includeAll = false)

    Returns a list of file extension that can be used when creating a FileDialog to open
    LDraw files. If \a includeAll is set, a match-everything filter \c * will be added to this
    list.
*/
QStringList QmlBrickStore::nameFiltersForLDraw(bool includeAll) const
{
    return DocumentIO::nameFiltersForLDraw(includeAll);
}

Document *QmlBrickStore::importBrickLinkStore(BrickLink::Store *store)
{
    return DocumentIO::importBrickLinkStore(store);
}

Document *QmlBrickStore::importBrickLinkOrder(BrickLink::Order *order)
{
    return DocumentIO::importBrickLinkOrder(order);
}

Document *QmlBrickStore::importBrickLinkCart(BrickLink::Cart *cart)
{
    return DocumentIO::importBrickLinkCart(cart);
}

Document *QmlBrickStore::importPartInventory(BrickLink::QmlItem item, BrickLink::QmlColor color,
                                             int multiply, BrickLink::Condition condition,
                                             BrickLink::Status extraParts,
                                             bool includeInstructions, bool includeAlternates,
                                             bool includeCounterParts)
{
    return Document::fromPartInventory(item.wrappedObject(), color.wrappedObject(), multiply,
                                       condition, extraParts,
                                       includeInstructions, includeAlternates, includeCounterParts);
}

/*! \qmlmethod void BrickStore::updateDatabase()

    Starts an asychronous database update in the background.
*/
void QmlBrickStore::updateDatabase()
{
    Application::inst()->updateDatabase();
}

Document *QmlBrickStore::activeDocument() const
{
    return ActionManager::inst()->activeDocument();
}

QmlThenable *QmlBrickStore::checkBrickLinkLogin()
{
    auto *thenable = new QmlThenable(qmlEngine(this));

    Application::inst()->checkBrickLinkLogin().then([thenable](bool ok) {
        thenable->callThen({ ok });
    });
    return thenable;
}

void QmlBrickStore::updateIconTheme(bool darkTheme)
{
    Application::inst()->setIconTheme(darkTheme ? Application::DarkTheme : Application::LightTheme);
}



QmlDocumentProxyModel::QmlDocumentProxyModel(QObject *parent)
    : QAbstractProxyModel(parent)
    , m_forceLayoutDelay(new QTimer(this))
    , m_columnModel(new QmlDocumentColumnModel(this))
{
    m_forceLayoutDelay->setInterval(100);
    m_forceLayoutDelay->setSingleShot(true);
    connect(m_forceLayoutDelay, &QTimer::timeout,
            this, &QmlDocumentProxyModel::forceLayout);
}

void QmlDocumentProxyModel::setDocument(Document *doc)
{
    beginResetModel();

    delete m_connectionContext;
    m_connectionContext = nullptr;

    m_doc = doc;
    setSourceModel(doc ? doc->model() : nullptr);

    if (m_doc) {
        m_connectionContext = new QObject(this);

        connect(m_doc, &Document::columnPositionChanged,
                m_connectionContext, [this](int /*li*/, int /*vi*/) {
            m_columnModel->beginResetModel();
            beginResetModel();
            update();
            endResetModel();
            m_columnModel->endResetModel();
            emitForceLayout(); //TODO: check if needed
        });
        connect(m_doc, &Document::columnSizeChanged,
                m_connectionContext, [this](int li, int /*size*/) {
            emit headerDataChanged(Qt::Horizontal, l2v[li], l2v[li]);
            emitForceLayout();
        });
        connect(m_doc, &Document::columnHiddenChanged,
                m_connectionContext, [this](int li, bool /*hidden*/) {
            emit headerDataChanged(Qt::Horizontal, l2v[li], l2v[li]);
            emitForceLayout();
        });
        connect(m_doc, &Document::columnLayoutChanged,
                m_connectionContext, [this]() {
            emitForceLayout();
        });

        connect(m_doc->model(), &QAbstractItemModel::dataChanged,
                m_connectionContext, [this](const QModelIndex &from, const QModelIndex &to, const QVector<int> &roles) {
            if (from.column() == to.column()) {
                emit dataChanged(mapFromSource(from), mapFromSource(to), roles);
            } else {
                emit dataChanged(index(from.row(), 0), index(to.row(), columnCount() - 1), roles);
            }
        });
        connect(m_doc->model(), &QAbstractItemModel::headerDataChanged,
                m_connectionContext, [this](Qt::Orientation o, int section, int role) {
            emit headerDataChanged(o, section < 0 ? -1 : l2v[section], role);
        });
        connect(m_doc->model(), &QAbstractItemModel::modelAboutToBeReset,
                m_connectionContext, [this]() {
            beginResetModel();
        });
        connect(m_doc->model(), &QAbstractItemModel::modelReset,
                m_connectionContext, [this]() {
            endResetModel();
        });
        connect(m_doc->model(), &QAbstractItemModel::layoutAboutToBeChanged,
                m_connectionContext, [this](const QList<QPersistentModelIndex> &, LayoutChangeHint hint) {
            emit layoutAboutToBeChanged({ }, hint);
        });
        connect(m_doc->model(), &QAbstractItemModel::layoutChanged,
                m_connectionContext, [this](const QList<QPersistentModelIndex> &, LayoutChangeHint hint) {
            emit layoutChanged({ }, hint);
        });
        connect(m_doc->model(), &QAbstractItemModel::rowsAboutToBeInserted,
                m_connectionContext, [this](const QModelIndex &, int start, int end) {
            beginInsertRows({ }, start, end);
        });
        connect(m_doc->model(), &QAbstractItemModel::rowsInserted,
                m_connectionContext, [this](const QModelIndex &, int, int) {
            endInsertRows();
        });
        connect(m_doc->model(), &QAbstractItemModel::rowsAboutToBeRemoved,
                m_connectionContext, [this](const QModelIndex &, int start, int end) {
            beginRemoveRows({ }, start, end);
        });
        connect(m_doc->model(), &QAbstractItemModel::rowsRemoved,
                m_connectionContext, [this](const QModelIndex &, int, int) {
            endRemoveRows();
        });
        connect(m_doc->model(), &QAbstractItemModel::rowsAboutToBeMoved,
                m_connectionContext, [this](const QModelIndex &, int sstart, int send, const QModelIndex &, int dstart) {
            beginMoveRows({ }, sstart, send, { }, dstart);
        });
        connect(m_doc->model(), &QAbstractItemModel::rowsMoved,
                m_connectionContext, [this](const QModelIndex &, int, int, const QModelIndex &, int) {
            endMoveRows();
        });
        connect(this, &QmlDocumentProxyModel::headerDataChanged,
                this, [this](Qt::Orientation orientation, int, int) {
            if (orientation != Qt::Horizontal)
                return;
            auto tl = m_columnModel->index(0);
            auto br = m_columnModel->index(m_columnModel->rowCount() - 1);
            emit m_columnModel->dataChanged(tl, br);
        });
    }
    update();
    endResetModel();
}

Document *QmlDocumentProxyModel::document() const
{
    return m_doc;
}


int QmlDocumentProxyModel::rowCount(const QModelIndex &parent) const
{
    return sourceModel()->rowCount(mapToSource(parent));
}

int QmlDocumentProxyModel::columnCount(const QModelIndex &parent) const
{
    return sourceModel()->columnCount(mapToSource(parent));
}

QModelIndex QmlDocumentProxyModel::index(int row, int column, const QModelIndex &) const
{
    auto sindex = sourceModel()->index(row, column < 0 ? -1 : v2l[column], { });
    return mapFromSource(sindex);
}

QModelIndex QmlDocumentProxyModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

QVariant QmlDocumentProxyModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid() && (role == Qt::UserRole + 3456)) {
        return logicalColumn(index.column());
    }
    QModelIndex sindex = mapToSource(index);
    return sourceModel()->data(sindex, role);
}

QVariant QmlDocumentProxyModel::headerData(int section, Qt::Orientation o, int role) const
{
    if ((o != Qt::Horizontal) || (section < 0) || (section >= m_doc->model()->columnCount()))
        return { };

    switch (role) {
    case Qt::SizeHintRole: {
        const auto &cd = m_doc->columnLayout().value(v2l[section]);
        return cd.m_size;
    }
    case Qt::CheckStateRole: {
        const auto &cd = m_doc->columnLayout().value(v2l[section]);
        return cd.m_hidden;
    }
    default:
        return sourceModel()->headerData(v2l[section], o, role);
    }
}

QModelIndex QmlDocumentProxyModel::mapToSource(const QModelIndex &index) const
{
    auto sindex = createSourceIndex(index.row(), index.column() < 0 ? -1 : v2l[index.column()],
                                    index.internalPointer());
    return sindex;
}

QModelIndex QmlDocumentProxyModel::mapFromSource(const QModelIndex &sindex) const
{
    auto index = createIndex(sindex.row(), sindex.column() < 0 ? -1 : l2v[sindex.column()],
                             sindex.internalPointer());
    return index;
}

int QmlDocumentProxyModel::logicalColumn(int visual) const
{
    return ((visual >= 0) && (visual < v2l.size())) ? v2l[visual] : -1;
}

int QmlDocumentProxyModel::visualColumn(int logical) const
{
    return ((logical >= 0) && (logical < l2v.size())) ? l2v[logical] : -1;
}

QHash<int, QByteArray> QmlDocumentProxyModel::roleNames() const
{
    auto hash = sourceModel()->roleNames();
    hash.insert(Qt::UserRole + 3456, "documentField");
    return hash;
}

QAbstractListModel *QmlDocumentProxyModel::columnModel()
{
    return m_columnModel;
}

void QmlDocumentProxyModel::update()
{
    if (!m_doc)
        return;

    const auto layout = m_doc->columnLayout();
    auto count = layout.count();
    v2l.resize(count);
    l2v.resize(count);

    for (int li = 0; li < layout.size(); ++li) {
        l2v[li] = layout[li].m_visualIndex;
        v2l[l2v[li]] = li;
    }
}

void QmlDocumentProxyModel::emitForceLayout()
{
    m_forceLayoutDelay->start();
}

void QmlDocumentProxyModel::internalMoveColumn(int viFrom, int viTo)
{
    int li = v2l[viFrom];
    m_doc->moveColumn(li, viFrom, viTo);
}

void QmlDocumentProxyModel::internalHideColumn(int vi, bool hidden)
{
    int li = v2l[vi];
    const auto &cd = m_doc->columnLayout().value(li);

    if (cd.m_hidden != hidden)
        m_doc->hideColumn(li, cd.m_hidden, hidden);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlDocumentColumnModel::QmlDocumentColumnModel(QmlDocumentProxyModel *proxyModel)
    : QAbstractListModel(proxyModel)
    , m_proxyModel(proxyModel)
{ }

int QmlDocumentColumnModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_proxyModel->columnCount();
}

QVariant QmlDocumentColumnModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return { };
    return m_proxyModel->headerData(index.row(), Qt::Horizontal, role);
}

QHash<int, QByteArray> QmlDocumentColumnModel::roleNames() const
{
    static QHash<int, QByteArray> roles = {
        { Qt::DisplayRole, "name" },
        { Qt::CheckStateRole, "hidden" },
    };
    return roles;
}

void QmlDocumentColumnModel::moveColumn(int viFrom, int viTo)
{
    m_proxyModel->internalMoveColumn(viFrom, viTo);
}

void QmlDocumentColumnModel::hideColumn(int vi, bool hidden)
{
    m_proxyModel->internalHideColumn(vi, hidden);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ColumnLayoutsModel::ColumnLayoutsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(Config::inst(), &Config::columnLayoutIdsChanged,
            this, &ColumnLayoutsModel::update);
    connect(Config::inst(), &Config::columnLayoutIdsOrderChanged,
            this, &ColumnLayoutsModel::update);
    connect(Config::inst(), &Config::columnLayoutNameChanged,
            this, &ColumnLayoutsModel::update);
    update();
}

int ColumnLayoutsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_idAndName.size());
}

QVariant ColumnLayoutsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return { };

    switch (role) {
    case Qt::DisplayRole:
        return m_idAndName.at(index.row()).second;
    case Qt::UserRole:
        return m_idAndName.at(index.row()).first;
    default:
        return { };
    }
}

QHash<int, QByteArray> ColumnLayoutsModel::roleNames() const
{
    return { { Qt::DisplayRole, "name" }, { Qt::UserRole, "id" } };
}

void ColumnLayoutsModel::update()
{
    beginResetModel();
    m_idAndName.clear();

    const auto specialCommands = Document::columnLayoutCommands();
    for (auto cmd : specialCommands) {
        auto id = Document::columnLayoutCommandId(cmd);
        auto name = Document::columnLayoutCommandName(cmd);
        m_idAndName.append({ id, name });
    }
    const auto userLayoutIds = Config::inst()->columnLayoutIds();
    if (!userLayoutIds.isEmpty())
        m_idAndName.append({ u""_qs, u"-"_qs });

    QMap<int, QString> orderedUserLayoutIds;
    for (const auto &layoutId : userLayoutIds)
        orderedUserLayoutIds.insert(Config::inst()->columnLayoutOrder(layoutId), layoutId);

    for (const auto &layoutId : qAsConst(orderedUserLayoutIds)) {
        auto name = Config::inst()->columnLayoutName(layoutId);
        m_idAndName.append({ layoutId, name });
    }
    endResetModel();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlSortFilterProxyModel::QmlSortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    connect(this, &QSortFilterProxyModel::rowsInserted, this, [this]() {
        emit countChanged(count());
    });
    connect(this, &QSortFilterProxyModel::rowsRemoved, this,  [this]() {
        emit countChanged(count());
    });
    connect(this, &QSortFilterProxyModel::filterRoleChanged, this, [this](int role) {
        emit filterRoleNameChanged(roleNames().value(role));
    });
    connect(this, &QSortFilterProxyModel::sortRoleChanged, this, [this](int role) {
        emit sortRoleNameChanged(roleNames().value(role));
    });
}

int QmlSortFilterProxyModel::count() const
{
    return rowCount();
}

QByteArray QmlSortFilterProxyModel::sortRoleName() const
{
    return roleNames().value(sortRole());
}

void QmlSortFilterProxyModel::setSortRoleName(const QByteArray &role)
{
    setSortRole(roleKey(role));
    invalidate();
}

QByteArray QmlSortFilterProxyModel::filterRoleName() const
{
    return roleNames().value(filterRole());
}

void QmlSortFilterProxyModel::setFilterRoleName(const QByteArray &role)
{
    setFilterRole(roleKey(role));
}

void QmlSortFilterProxyModel::setSortColumn(int newSortColumn)
{
    if (sortColumn() != newSortColumn) {
        sort(newSortColumn, sortOrder());
        emit sortColumnChanged(newSortColumn);
    }
}

void QmlSortFilterProxyModel::setSortOrder(Qt::SortOrder newSortOrder)
{
    if (sortOrder() != newSortOrder) {
        sort(sortColumn(), newSortOrder);
        emit sortOrderChanged(newSortOrder);
    }
}

QString QmlSortFilterProxyModel::filterString() const
{
    return filterRegularExpression().pattern();
}

void QmlSortFilterProxyModel::setFilterString(const QString &filter)
{
    switch (m_filterSyntax) {
    case RegularExpression:
        setFilterRegularExpression(filter);
        break;
    case Wildcard:
        setFilterRegularExpression(QRegularExpression::fromWildcard(filter));
        break;
    case FixedString:
        setFilterFixedString(filter);
        break;
    }
}

QmlSortFilterProxyModel::FilterSyntax QmlSortFilterProxyModel::filterSyntax() const
{
    return m_filterSyntax;
}

void QmlSortFilterProxyModel::setFilterSyntax(FilterSyntax syntax)
{
    if (m_filterSyntax != syntax) {
        m_filterSyntax = syntax;
        emit filterSyntaxChanged(syntax);
        //setFilterString({ });
    }
}

QHash<int, QByteArray> QmlSortFilterProxyModel::roleNames() const
{
    if (auto source = sourceModel())
        return source->roleNames();
    return { };
}

int QmlSortFilterProxyModel::roleKey(const QByteArray &role) const
{
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        if (it.value() == role)
            return it.key();
    }
    return -1;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


void QmlClipboard::clear(QClipboard::Mode mode)
{
    QGuiApplication::clipboard()->clear(mode);
}

QString QmlClipboard::text(QClipboard::Mode mode) const
{
    return QGuiApplication::clipboard()->text(mode);
}

void QmlClipboard::setText(const QString &text, QClipboard::Mode mode)
{
    QGuiApplication::clipboard()->setText(text, mode);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QFont QmlUtility::monospaceFont() const
{
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlThenable::QmlThenable(QJSEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

QmlThenable::~QmlThenable()
{ }

void QmlThenable::then(const QJSValue &function)
{
    m_thenable = true;
    m_function = function;
    if (!m_function.isCallable())
        qWarning() << "The argument for QmlThenable::then() is not callable";
}

void QmlThenable::callThen(const QVariantList &arguments)
{
    if (!m_thenable) { // not returned to JS yet?
        QMetaObject::invokeMethod(this, [this, arguments]() {
            if (!m_thenable) // nobody called then() on the JS side
                deleteLater();
            else
                callThenInternal(arguments);
        }, Qt::QueuedConnection);
    } else {
        callThenInternal(arguments);
    }
}

void QmlThenable::callThenInternal(const QVariantList &arguments)
{
    if (m_function.isCallable()) {
        QJSValueList vl;
        for (const auto &arg : arguments)
            vl << m_engine->toScriptValue(arg);
        auto result = m_function.call(vl);
        if (result.isError()) {
            qWarning() << "QmlThenable::then() failed:" << result.toString();
        }
    }
    QQmlEngine::setObjectOwnership(this, QQmlEngine::JavaScriptOwnership);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


namespace {
enum {
    TypeRole = Qt::UserRole + 1,
    CategoryRole,
    MessageRole,
    LineRole,
    FileRole,
};
}

QmlDebugLogModel *QmlDebugLogModel::s_inst = nullptr;

QmlDebugLogModel::QmlDebugLogModel(QObject *parent)
    : QAbstractListModel(parent)
{ }

QmlDebugLogModel *QmlDebugLogModel::inst()
{
    if (!s_inst)
        s_inst = new QmlDebugLogModel(qApp);
    return s_inst;
}

int QmlDebugLogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_logs.size();
}

QVariant QmlDebugLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return { };

    const Log &log = m_logs.at(index.row());

    switch (role) {
    case TypeRole    : return int(log.type);
    case LineRole    : return log.line;
    case FileRole    : return log.file;
    case CategoryRole: return log.category;
    case MessageRole : return log.message;
    }
    return { };

}

QHash<int, QByteArray> QmlDebugLogModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { TypeRole, "type" },
        { CategoryRole, "category" },
        { MessageRole, "message" },
        { LineRole, "line" },
        { FileRole, "file" },
    };
    return roles;
}

void QmlDebugLogModel::append(QtMsgType type, const QString &category, const QString &file,
                              int line, const QString &message)
{
    beginInsertRows({ }, m_logs.size(), m_logs.size());
    m_logs.emplace_back(type, line, category, file, message);
    endInsertRows();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlDebug::QmlDebug(QObject *parent)
    : QObject(parent)
{
    m_showTracers = (qEnvironmentVariableIntValue("BS_SHOW_TRACERS") == 1);
}

bool QmlDebug::showTracers() const
{
    return m_showTracers;
}

void QmlDebug::setShowTracers(bool newShowTracers)
{
    if (m_showTracers != newShowTracers) {
        m_showTracers = newShowTracers;
        emit showTracersChanged(newShowTracers);
    }
}

QAbstractListModel *QmlDebug::log() const
{
    return QmlDebugLogModel::inst();
}


#include "moc_brickstore_wrapper.cpp"