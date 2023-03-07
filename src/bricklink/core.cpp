// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <cstdio>
#include <cstdlib>
#include <array>

#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QDebug>
#include <QThread>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRunnable>

#include "utility/appstatistics.h"
#include "utility/q5hashfunctions.h"
#include "utility/utility.h"
#include "utility/exception.h"
#include "utility/transfer.h"

#include "bricklink/category.h"
#include "bricklink/changelogentry.h"
#include "bricklink/color.h"
#include "bricklink/core.h"
#include "bricklink/global.h"
#include "bricklink/item.h"
#include "bricklink/itemtype.h"
#include "bricklink/lot.h"
#include "bricklink/partcolorcode.h"
#include "bricklink/picture.h"
#include "bricklink/priceguide.h"
#if !defined(BS_BACKEND)
#  include "bricklink/cart.h"
#  include "bricklink/order.h"
#  include "bricklink/store.h"
#  include "bricklink/wantedlist.h"
#endif

Q_LOGGING_CATEGORY(LogResolver, "resolver")
Q_LOGGING_CATEGORY(LogCache, "cache")
Q_LOGGING_CATEGORY(LogSql, "sql")


namespace BrickLink {

QUrl Core::urlForInventoryRequest()
{
    return u"https://www.bricklink.com/catalogInvAdd.asp"_qs;
}

QUrl Core::urlForWantedListUpload()
{
    return u"https://www.bricklink.com/wantedXML.asp"_qs;
}

QUrl Core::urlForInventoryUpload()
{
    return u"https://www.bricklink.com/invXML.asp"_qs;
}

QUrl Core::urlForInventoryUpdate()
{
    return u"https://www.bricklink.com/invXML.asp#update"_qs;
}

QUrl Core::urlForCatalogInfo(const Item *item, const Color *color)
{
    QUrl url;
    if (item && item->itemType()) {
        url = u"https://www.bricklink.com/catalogItem.asp"_qs;
        QUrlQuery query {
            { QString(QLatin1Char(item->itemTypeId())), QLatin1String(item->id()) }
        };
        if (item->itemType()->hasColors() && color)
            query.addQueryItem(u"C"_qs, QString::number(color->id()));
        url.setQuery(query);
    }
    return url;
}

QUrl Core::urlForPriceGuideInfo(const Item *item, const Color *color)
{
    QUrl url;
    if (item && item->itemType()) {
        url = u"https://www.bricklink.com/catalogPG.asp"_qs;
        QUrlQuery query {
            { QString(QLatin1Char(item->itemTypeId())), Utility::urlQueryEscape(item->id()) }
        };
        if (item->itemType()->hasColors() && color)
            query.addQueryItem(u"colorID"_qs, QString::number(color->id()));
        url.setQuery(query);
    }
    return url;
}

QUrl Core::urlForLotsForSale(const Item *item, const Color *color)
{
    QUrl url;
    if (item && item->itemType()) {
        url = u"https://www.bricklink.com/search.asp"_qs;
        QUrlQuery query {
            { u"viewFrom"_qs, u"sa"_qs },
            { u"itemType"_qs, QString(QLatin1Char(item->itemTypeId())) },
        };

        // workaround for BL not accepting the -X suffix for sets, instructions and boxes
        QString id = QString::fromLatin1(item->id());
        char itt = item->itemTypeId();

        if (itt == 'S' || itt == 'I' || itt == 'O') {
            auto pos = id.lastIndexOf(u'-');
            if (pos >= 0)
                id.truncate(pos);
        }
        query.addQueryItem(u"q"_qs, Utility::urlQueryEscape(id));

        if (item->itemType()->hasColors() && color)
            query.addQueryItem(u"colorID"_qs, QString::number(color->id()));
        url.setQuery(query);
    }
    return url;
}

QUrl Core::urlForAppearsInSets(const Item *item, const Color *color)
{
    QUrl url;
    if (item && item->itemType()) {
        url = u"https://www.bricklink.com/catalogItemIn.asp"_qs;
        QUrlQuery query {
            { QString(QLatin1Char(item->itemTypeId())), Utility::urlQueryEscape(item->id()) },
            { u"in"_qs, u"S"_qs },
        };
        if (item->itemType()->hasColors() && color)
            query.addQueryItem(u"colorID"_qs, QString::number(color->id()));
        url.setQuery(query);
    }
    return url;
}

QUrl Core::urlForStoreItemDetail(uint lotId)
{
    QUrl url = u"https://www.bricklink.com/inventory_detail.asp"_qs;
    url.setQuery({ { u"invID"_qs, QString::number(lotId) } } );
    return url;
}

QUrl Core::urlForStoreItemSearch(const Item *item, const Color *color)
{
    QUrl url;
    if (item && item->itemType()) {
        url = u"https://www.bricklink.com/inventory_detail.asp?"_qs;
        QUrlQuery query;
        query.addQueryItem(u"catType"_qs, QString(QLatin1Char(item->itemTypeId())));
        QString queryTerm = QString::fromLatin1(item->id());
        if (queryTerm.contains(u'-')) {
            queryTerm = item->name();
            queryTerm.remove(u'(');
            queryTerm.remove(u')');
        }
        if (item->itemType()->hasColors() && color)
            queryTerm = color->name() + u' ' + queryTerm;
        query.addQueryItem(u"q"_qs, Utility::urlQueryEscape(queryTerm));
        url.setQuery(query);
    }
    return url;
}

QUrl Core::urlForOrderDetails(const QString &orderId)
{
    QUrl url = u"https://www.bricklink.com/orderDetail.asp"_qs;
    url.setQuery({ { u"ID"_qs, Utility::urlQueryEscape(orderId) } });
    return url;
}

QUrl Core::urlForShoppingCart(int shopId)
{
    QUrl url = u"https://www.bricklink.com/v2/globalcart.page"_qs;
    url.setQuery({ { u"sid"_qs, QString::number(shopId) } });
    return url;
}

QUrl Core::urlForWantedList(uint wantedListId)
{
    QUrl url = u"https://www.bricklink.com/v2/wanted/edit.page"_qs;
    url.setQuery({ { u"wantedMoreID"_qs, QString::number(wantedListId) } });
    return url;
}

const QImage Core::noImage(const QSize &s) const
{
    uint key = uint(s.width() << 16) | uint(s.height());
    QImage img = m_noImageCache.value(key);

    if (img.isNull()) {
        img = m_noImageIcon.pixmap(s).toImage();
        m_noImageCache.insert(key, img);
    }
    return img;
}



QString Core::dataPath() const
{
    return m_datadir;
}

QString Core::dataFileName(QStringView fileName, const Item *item, const Color *color) const
{
    // Avoid huge directories with 1000s of entries.
    // sse4.2 is only used if a seed value is supplied
    // please note: Qt6's qHash is incompatible
    uchar hash = q5Hash(QString::fromLatin1(item->id()), 42) & 0xff;

    QString p = m_datadir + QLatin1Char(item->itemTypeId()) + u'/' + (hash < 0x10 ? u"0" : u"")
            + QString::number(hash, 16) + u'/' + QLatin1String(item->id()) + u'/'
            + (color ? QString::number(color->id()) : QString()) + (color ? u"/" : u"")
            + fileName;

    return p;
}

QFile *Core::dataReadFile(QStringView fileName, const Item *item, const Color *color) const
{
    auto f = new QFile(dataFileName(fileName, item, color));
    f->open(QIODevice::ReadOnly);
    return f;
}

QSaveFile *Core::dataSaveFile(QStringView fileName, const Item *item, const Color *color) const
{
    auto p = dataFileName(fileName, item, color);

    if (!QDir(fileName.isEmpty() ? p : p.left(p.size() - int(fileName.size()))).mkpath(u"."_qs))
        return nullptr;

    auto f = new QSaveFile(p);
    if (!f->open(QIODevice::WriteOnly)) {
        qCWarning(LogCache) << "BrickLink::Core::dataSaveFile failed to open" << f->fileName()
                            << "for writing:" << f->errorString();
    }
    return f;
}

void Core::setCredentials(const QPair<QString, QString> &credentials)
{
    if (m_credentials != credentials) {
        bool wasAuthenticated = m_authenticated;
        m_authenticated = false;
        if (m_authenticatedTransfer)
            m_authenticatedTransfer->abortAllJobs();
        bool newUserId = (credentials.first != m_credentials.first);
        m_credentials = credentials;
        if (wasAuthenticated) {
            emit authenticationChanged(false);

            QUrl url(u"https://www.bricklink.com/ajax/renovate/loginandout.ajax"_qs);
            QUrlQuery q;
            q.addQueryItem(u"do_logout"_qs, u"true"_qs);
            url.setQuery(q);

            auto logoutJob = TransferJob::get(url);
            m_authenticatedTransfer->retrieve(logoutJob, true);
        }
        if (newUserId)
            emit userIdChanged(m_credentials.first);
    }
}

QString Core::userId() const
{
    return m_credentials.first;
}

bool Core::isAuthenticated() const
{
    return m_authenticated;
}

void Core::retrieveAuthenticated(TransferJob *job)
{
    job->setNoRedirects(true);

    if (!m_authenticated) {
        if (!m_loginJob) {
            if (m_credentials.first.isEmpty() || m_credentials.second.isEmpty()) {
                QMetaObject::invokeMethod(this, [job]() { job->abort(); });
                qWarning() << "Aborting transfer due to missing credentials";
                return;
            }

            QUrl url(u"https://www.bricklink.com/ajax/renovate/loginandout.ajax"_qs);
            QUrlQuery q;
            q.addQueryItem(u"userid"_qs,          Utility::urlQueryEscape(m_credentials.first));
            q.addQueryItem(u"password"_qs,        Utility::urlQueryEscape(m_credentials.second));
            q.addQueryItem(u"keepme_loggedin"_qs, u"1"_qs);
            url.setQuery(q);

            m_loginJob = TransferJob::post(url, nullptr, true /* no redirects */);
            m_authenticatedTransfer->retrieve(m_loginJob, true);
        }
        m_jobsWaitingForAuthentication << job;
    } else {
        m_authenticatedTransfer->retrieve(job);
    }
}

void Core::retrieve(TransferJob *job, bool highPriority)
{
    m_transfer->retrieve(job, highPriority);
}


Core *Core::s_inst = nullptr;

Core *Core::create(const QString &dataDir, const QString &updateUrl, quint64 physicalMem)
{
    if (!s_inst) {
        QString test = dataDir;

        if (!test.isEmpty()) {
            QFileInfo fi(test);

            if (!fi.exists()) {
                QDir(test).mkpath(u"."_qs);
                fi.refresh();
            }
            if (!fi.exists() || !fi.isDir() || !fi.isReadable() || !fi.isWritable())
                test.clear();
        }

        if (test.isEmpty())
            throw Exception(tr("Data directory \'%1\' is not both read- and writable.")).arg(dataDir);

        s_inst = new Core(dataDir, updateUrl, physicalMem);
    }
    return s_inst;
}

Core::Core(const QString &datadir, const QString &updateUrl, quint64 physicalMem)
    : m_datadir(QDir::cleanPath(QDir(datadir).absolutePath()) + u'/')
    , m_noImageIcon(QIcon::fromTheme(u"image-missing-large"_qs))
    , m_transfer(new Transfer(this))
    , m_authenticatedTransfer(new Transfer(this))
    , m_database(new Database(updateUrl, this))
#if !defined(BS_BACKEND)
    , m_store(new Store(this))
    , m_orders(new Orders(this))
    , m_carts(new Carts(this))
    , m_wantedLists(new WantedLists(this))
    , m_priceGuideCache(new PriceGuideCache(this))
    , m_pictureCache(new PictureCache(this, physicalMem))
#endif
{
#if defined(BS_BACKEND)
    Q_UNUSED(physicalMem)
#endif

    m_transferStatId = AppStatistics::inst()->addSource(u"HTTP requests"_qs);

    //TODO: See if we cannot make this cancellation a bit more robust.
    //      Right now, cancelTransfers() is fully async. We could potentially detect when all
    //      TransferJobs are handled, but the disk-load runnables are very tricky with their
    //      cross-thread invokeMethod().
    //      Right now, we are cancelling before the download even starts, which should give us
    //      plenty of iterations through the event loop to handle all the cancelled jobs.

    connect(m_database.get(), &Database::databaseAboutToBeReset,
            this, &Core::cancelTransfers);
    connect(m_database.get(), &Database::databaseReset,
            this, [this]() {
        m_priceGuideCache->clearCache();
        m_pictureCache->clearCache();
    });

    connect(m_transfer, &Transfer::finished,
            this, &Core::transferFinished);
    connect(m_transfer, &Transfer::overallProgress,
            this, [this](int p, int t) {
        AppStatistics::inst()->update(m_transferStatId, t - p);
        emit transferProgress(p, t);
    });

    connect(m_authenticatedTransfer, &Transfer::finished,
            this, [this](TransferJob *job) {
        if (!job) {
            return;
        } else if (job == m_loginJob) {
            m_loginJob = nullptr;

            QString error;

            if (job->responseCode() == 200 && job->data()) {
                auto json = QJsonDocument::fromJson(*job->data());
                if (!json.isNull()) {
                    bool wasAuthenticated = m_authenticated;
                    m_authenticated = (json[u"returnCode"].toInt() == 0);
                    if (!m_authenticated)
                        error = json[u"returnMessage"].toString();

                    if (wasAuthenticated != m_authenticated)
                        emit authenticationChanged(m_authenticated);
                }
            } else {
                error = job->errorString();
            }
            if (!error.isEmpty())
                emit authenticationFailed(m_credentials.first, error);

            for (TransferJob *authJob : std::as_const(m_jobsWaitingForAuthentication))
                m_authenticatedTransfer->retrieve(authJob);
            m_jobsWaitingForAuthentication.clear();

            if (!m_authenticated)
                m_authenticatedTransfer->abortAllJobs();
        } else {
            if (job->responseCode() == 302 &&
                    (job->redirectUrl().toString().contains(u"v2/login.page")
                     || job->redirectUrl().toString().contains(u"login.asp?"))) {
                m_authenticated = false;
                emit authenticationChanged(m_authenticated);

                job->resetForReuse();
                QMetaObject::invokeMethod(this, [=, this]() {
                    retrieveAuthenticated(job);
                }, Qt::QueuedConnection);
            } else {
                emit authenticatedTransferFinished(job);
            }
        }
    });
    connect(m_authenticatedTransfer, &Transfer::overallProgress,
            this, &Core::authenticatedTransferOverallProgress);
    connect(m_authenticatedTransfer, &Transfer::progress,
            this, &Core::authenticatedTransferProgress);
    connect(m_authenticatedTransfer, &Transfer::started,
            this, &Core::authenticatedTransferStarted);
}

Core::~Core()
{
    cancelTransfers();

    m_priceGuideCache->clearCache();
    m_pictureCache->clearCache();

    s_inst = nullptr;
}

void Core::setUpdateIntervals(const QMap<QByteArray, int> &intervals)
{
    m_database->setUpdateInterval(intervals["Database"]);
    m_pictureCache->setUpdateInterval(intervals["Picture"]);
    m_priceGuideCache->setUpdateInterval(intervals["PriceGuide"]);
}

QString Core::countryIdFromName(const QString &name) const
{
    // BrickLink doesn't use the standard ISO country names...
    static const std::array brickLinkCountries = {
        "AF Afghanistan",
        "AL Albania",
        "DZ Algeria",
        "AD Andorra",
        "AO Angola",
        "AI Anguilla",
        "AG Antigua and Barbuda",
        "AR Argentina",
        "AM Armenia",
        "AW Aruba",
        "AU Australia",
        "AT Austria",
        "AZ Azerbaijan",
        "BS Bahamas",
        "BH Bahrain",
        "BD Bangladesh",
        "BB Barbados",
        "BY Belarus",
        "BE Belgium",
        "BZ Belize",
        "BJ Benin",
        "BM Bermuda",
        "BT Bhutan",
        "BO Bolivia",
        "BA Bosnia and Herzegovina",
        "BW Botswana",
        "BR Brazil",
        "IO British Indian Ocean Territory",
        "BN Brunei",
        "BG Bulgaria",
        "BF Burkina Faso",
        "BI Burundi",
        "KH Cambodia",
        "CM Cameroon",
        "CA Canada",
        "CV Cape Verde",
        "BQ Caribbean Netherlands",
        "KY Cayman Islands",
        "CF Central African Republic",
        "TD Chad",
        "CL Chile",
        "CN China",
        "CO Colombia",
        "KM Comoros",
        "CG Congo",
        "CD Congo (DRC)",
        "CK Cook Islands",
        "CR Costa Rica",
        "CI Cote D\'Ivoire",
        "HR Croatia",
        "CW Curacao",
        "CY Cyprus",
        "CZ Czech Republic",
        "DK Denmark",
        "DJ Djibouti",
        "DM Dominica",
        "DO Dominican Republic",
        "TL East Timor",
        "EC Ecuador",
        "EG Egypt",
        "SV El Salvador",
        "GQ Equatorial Guinea",
        "ER Eritrea",
        "EE Estonia",
        "ET Ethiopia",
        "FK Falkland Islands (Islas Malvinas)",
        "FO Faroe Islands",
        "FJ Fiji",
        "FI Finland",
        "FR France",
        "PF French Polynesia",
        "GA Gabon",
        "GM Gambia",
        "GE Georgia",
        "DE Germany",
        "GH Ghana",
        "GI Gibraltar",
        "GR Greece",
        "GL Greenland",
        "GD Grenada",
        "GT Guatemala",
        "GN Guinea",
        "GW Guinea-Bissau",
        "GY Guyana",
        "HT Haiti",
        "HN Honduras",
        "HK Hong Kong",
        "HU Hungary",
        "IS Iceland",
        "IN India",
        "ID Indonesia",
        "IQ Iraq",
        "IE Ireland",
        "IL Israel",
        "IT Italy",
        "JM Jamaica",
        "JP Japan",
        "JO Jordan",
        "KZ Kazakhstan",
        "KE Kenya",
        "KI Kiribati",
        "KW Kuwait",
        "KG Kyrgyzstan",
        "LA Laos",
        "LV Latvia",
        "LB Lebanon",
        "LS Lesotho",
        "LR Liberia",
        "LI Liechtenstein",
        "LT Lithuania",
        "LU Luxembourg",
        "LY Lybia",
        "MO Macau",
        "MK Macedonia",
        "MG Madagascar",
        "MW Malawi",
        "MY Malaysia",
        "MV Maldives",
        "ML Mali",
        "MT Malta",
        "MH Marshall Islands",
        "MR Mauritania",
        "MU Mauritius",
        "YT Mayotte",
        "MX Mexico",
        "FM Micronesia",
        "MD Moldova",
        "MC Monaco",
        "MN Mongolia",
        "ME Montenegro",
        "MS Montserrat",
        "MA Morocco",
        "MZ Mozambique",
        "MM Myanmar",
        "NA Namibia",
        "NR Nauru",
        "NP Nepal",
        "NL Netherlands",
        "NC New Caledonia",
        "NZ New Zealand",
        "NI Nicaragua",
        "NE Niger",
        "NU Niue",
        "NF Norfolk Island",
        "NO Norway",
        "OM Oman",
        "PK Pakistan",
        "PW Palau",
        "PA Panama",
        "PG Papua new Guinea",
        "PY Paraguay",
        "PE Peru",
        "PH Philippines",
        "PN Pitcairn Islands",
        "PL Poland",
        "PT Portugal",
        "QA Qatar",
        "RO Romania",
        "RU Russia",
        "RW Rwanda",
        "WS Samoa",
        "SM San Marino",
        "ST Sao Tome and Principe",
        "SA Saudi Arabia",
        "SN Senegal",
        "RS Serbia",
        "SC Seychelles",
        "SL Sierra Leone",
        "SG Singapore",
        "SX Sint Maarten",
        "SK Slovakia",
        "SI Slovenia",
        "SB Solomon Islands",
        "SO Somalia",
        "ZA South Africa",
        "GS South Georgia",
        "KR South Korea",
        "ES Spain",
        "LK Sri Lanka",
        "SH St. Helena",
        "KN St. Kitts and Nevis",
        "LC St. Lucia",
        "PM St. Pierre and Miquelon",
        "VC St. Vincent and the Grenadines",
        "SD Sudan",
        "SR Suriname",
        "SJ Svalbard and Jan Mayen",
        "SZ Swaziland",
        "SE Sweden",
        "CH Switzerland",
        "TW Taiwan",
        "TJ Tajikistan",
        "TZ Tanzania",
        "TH Thailand",
        "TG Togo",
        "TO Tonga",
        "TT Trinidad and Tobago",
        "TN Tunisia",
        "TR Turkey",
        "TM Turkmenistan",
        "TC Turks and Caicos Islands",
        "TV Tuvalu",
        "UG Uganda",
        "UA Ukraine",
        "AE United Arab Emirates",
        "UK United Kingdom",
        "UY Uruguay",
        "US USA",
        "UZ Uzbekistan",
        "VU Vanuatu",
        "VA Vatican City State",
        "VE Venezuela",
        "VN Vietnam",
        "VG Virgin Islands (British)",
        "WF Wallis and Futuna",
        "YE Yemen",
        "ZM Zambia",
        "ZW Zimbabwe",
    };

    if (!name.isEmpty()) {
        for (const auto &country : brickLinkCountries) {
            QString countryStr = QString::fromLatin1(country);
            if (countryStr.mid(3) == name)
                return countryStr.left(2);
        }
    }
    return { };
}

QString Core::itemHtmlDescription(const Item *item, const Color *color, const QColor &highlight)
{
    if (item) {
        QString cs;
        if (!QByteArray("MP").contains(item->itemTypeId())) {
            cs = cs + uR"(<i><font color=")" + Utility::textColor(highlight).name() %
                    uR"(" style="background-color: )" + highlight.name() + uR"(;">&nbsp;)" %
                    item->itemType()->name() + uR"(&nbsp;</font></i>&nbsp;&nbsp;)";
        }
        if (color && color->id()) {
            QColor c = color->color();
            cs = cs + uR"(<b><font color=")" + Utility::textColor(c).name() %
                    uR"(" style="background-color: )" + c.name() + uR"(;">&nbsp;)" %
                    color->name() + uR"(&nbsp;</font></b>&nbsp;&nbsp;)";
        }

        return u"<center><b>" + QLatin1String(item->id()) + u"</b>&nbsp; " + cs %
                item->name() + u"</center>";
    } else {
        return { };
    }
}

const Category *Core::category(uint id) const
{
    auto it = std::lower_bound(categories().cbegin(), categories().cend(), id);
    if ((it != categories().cend()) && (*it == id))
        return &(*it);
    return nullptr;
}

const Color *Core::color(uint id) const
{
    auto it = std::lower_bound(colors().cbegin(), colors().cend(), id);
    if ((it != colors().cend()) && (*it == id))
        return &(*it);
    return nullptr;
}

const Color *Core::colorFromName(const QString &name) const
{
    if (name.isEmpty())
        return nullptr;

    auto it = std::find_if(colors().cbegin(), colors().cend(), [name](const auto &color) {
        return !color.name().compare(name, Qt::CaseInsensitive);
    });
    if (it != colors().cend())
        return &(*it);
    return nullptr;
}


const Color *Core::colorFromLDrawId(int ldrawId) const
{
    auto it = std::find_if(colors().cbegin(), colors().cend(), [ldrawId](const auto &color) {
        return (color.ldrawId() == ldrawId);
    });
    if (it != colors().cend())
        return &(*it);

    const auto &extraColors = m_database->m_ldrawExtraColors;

    it = std::find_if(extraColors.cbegin(), extraColors.cend(), [ldrawId](const auto &color) {
        return (color.ldrawId() == ldrawId);
    });
    if (it != extraColors.cend())
        return &(*it);

    return nullptr;
}


const ItemType *Core::itemType(char id) const
{
    auto it = std::lower_bound(itemTypes().cbegin(), itemTypes().cend(), id);
    if ((it != itemTypes().cend()) && (*it == id))
        return &(*it);
    return nullptr;
}

const Item *Core::item(char tid, const QByteArray &id) const
{
    auto needle = std::make_pair(tid, id);
    auto it = std::lower_bound(items().cbegin(), items().cend(), needle);
    if ((it != items().cend()) && (*it == needle))
        return &(*it);
    return nullptr;
}

const Item *Core::item(const std::string &tids, const QByteArray &id) const
{
    for (const char &tid : tids) {
        auto needle = std::make_pair(tid, id);
        auto it = std::lower_bound(items().cbegin(), items().cend(), needle);
        if ((it != items().cend()) && (*it == needle))
            return &(*it);
    }
    return nullptr;
}

const PartColorCode *Core::partColorCode(uint id)
{
    auto it = std::lower_bound(pccs().cbegin(), pccs().cend(), id);
    if ((it != pccs().cend()) && (*it == id))
        return &(*it);
    return nullptr;
}

void Core::cancelTransfers()
{
    if (m_transfer)
        m_transfer->abortAllJobs();
    if (m_authenticatedTransfer)
        m_authenticatedTransfer->abortAllJobs();
}

bool Core::applyChangeLog(const Item *&item, const Color *&color, const Incomplete *inc)
{
    if (!inc)
        return false;

    // there are a items that changed their name multiple times, so we have to loop (e.g. 3069bpb78)

    if (!item) {
        QByteArray itemTypeAndId = inc->m_itemtype_id + inc->m_item_id;
        if (!inc->m_itemtype_name.isEmpty())
            itemTypeAndId[0] = inc->m_itemtype_name.at(0).toUpper().toLatin1();

        while (!item) {
            auto it = std::lower_bound(itemChangelog().cbegin(), itemChangelog().cend(), itemTypeAndId);
            if ((it == itemChangelog().cend()) || (*it != itemTypeAndId))
                break;
            item = core()->item(it->toItemTypeId(), it->toItemId());
            if (!item)
                itemTypeAndId = it->toItemTypeId() + it->toItemId();
        }
    }
    if (!color) {
        uint colorId = inc->m_color_id;

        while (!color) {
            auto it = std::lower_bound(colorChangelog().cbegin(), colorChangelog().cend(), colorId);
            if ((it == colorChangelog().cend()) || (*it != colorId))
                break;
            color = core()->color(it->toColorId());
            if (!color)
                colorId = it->toColorId();
        }
    }

    return (item && color);
}

Core::ResolveResult Core::resolveIncomplete(Lot *lot)
{
    Incomplete *inc = lot->isIncomplete();

    if (!inc)
        return ResolveResult::Direct;

    const Item *item = nullptr;
    const Color *color = nullptr;

    if ((inc->m_itemtype_id != ItemType::InvalidId) && !inc->m_item_id.isEmpty())
        item = core()->item(inc->m_itemtype_id, inc->m_item_id);

    if (inc->m_color_id != Color::InvalidId)
        color = core()->color(inc->m_color_id);

    if (item)
        lot->setItem(item);
    if (color)
        lot->setColor(color);
    if (lot->item() && lot->color())
        return ResolveResult::Direct;

    bool ok = applyChangeLog(item, color, inc);

    if (ok) {
        qCInfo(LogResolver).nospace() << " [ OK ] "
                                      << (inc->m_itemtype_id ? inc->m_itemtype_id : '?')
                                      << '-' << inc->m_item_id.constData() << " (" << int(inc->m_color_id) << ')'
                                      << " -> "
                                      << item->itemTypeId()
                                      << '-' << item->id().constData() << " (" << color->id() << ')';
    } else {
        qCWarning(LogResolver).nospace() << " [FAIL] "
                                         << (inc->m_itemtype_id ? inc->m_itemtype_id : '?')
                                         << '-' << inc->m_item_id.constData() << " (" << int(inc->m_color_id) << ')';

        if (item) {
            inc->m_item_id.clear();
            inc->m_item_name.clear();
            inc->m_itemtype_id = ItemType::InvalidId;
            inc->m_itemtype_name.clear();
            inc->m_category_id = Category::InvalidId;
            inc->m_category_name.clear();
        }
        if (color) {
            inc->m_color_id = Color::InvalidId;
            inc->m_color_name.clear();
        }
    }
    if (item)
        lot->setItem(item);
    if (color)
        lot->setColor(color);

    Q_ASSERT(ok == !lot->isIncomplete());
    return ok ? ResolveResult::ChangeLog : ResolveResult::Fail;
}

QSize Core::standardPictureSize() const
{
    return QSize { 80, 60 };
}

} // namespace BrickLink

#include "moc_core.cpp"

