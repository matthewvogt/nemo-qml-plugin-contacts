#ifndef SEASIDECACHE_H
#define SEASIDECACHE_H

#include <qtcontacts-extensions.h>
#include <QContactStatusFlags>

#include <QContact>
#include <QContactId>

#include <QAbstractListModel>

// Provide enough of SeasideCache's interface to support SeasideFilteredModel

QTCONTACTS_USE_NAMESPACE

class SeasidePerson;

class SeasideCache : public QObject
{
    Q_OBJECT
public:
    enum FilterType {
        FilterNone,
        FilterAll,
        FilterFavorites,
        FilterOnline,
        FilterTypesCount
    };

    enum FetchDataType {
        FetchNone = 0,
        FetchAccountUri = (1 << 0),
        FetchPhoneNumber = (1 << 1),
        FetchEmailAddress = (1 << 2),
        FetchOrganization = (1 << 3),
        FetchTypesMask = (FetchAccountUri |
                          FetchPhoneNumber |
                          FetchEmailAddress |
                          FetchOrganization)
    };

    enum DisplayLabelOrder {
        FirstNameFirst = 0,
        LastNameFirst
    };

    enum ContactState {
        ContactAbsent,
        ContactPartial,
        ContactRequested,
        ContactComplete
    };

    enum {
        HasValidOnlineAccount = (QContactStatusFlags::IsOnline << 1)
    };

    struct CacheItem;
    struct ItemData
    {
        virtual ~ItemData() {}

        virtual void displayLabelOrderChanged(DisplayLabelOrder order) = 0;

        virtual void updateCacheContact(SeasideCache::CacheItem *item, const QContact &contact) = 0;

        virtual void constituentsFetched(const QList<int> &ids) = 0;
        virtual void mergeCandidatesFetched(const QList<int> &ids) = 0;
    };

    struct ItemListener
    {
        virtual ~ItemListener() {}

        virtual void itemUpdated(CacheItem *) {};
        virtual void itemAboutToBeRemoved(CacheItem *) {};

        ItemListener *next;
    };

    struct CacheItem
    {
        CacheItem()
            : iid(0), statusFlags(0), contactState(ContactAbsent), listeners(0)
        {
        }

        CacheItem(const QContact &contact)
            : iid(internalId(contact)), statusFlags(contact.detail<QContactStatusFlags>().flagsValue()), contactState(ContactComplete), listeners(0)
        {
            QDataStream os(&contactData, QIODevice::WriteOnly);
            os << contact;
        }

        QContactId apiId() const { return SeasideCache::apiId(iid); }

        ItemListener *listener(void *) { return 0; }

        ItemListener *appendListener(ItemListener *listener, void *) { listeners = listener; listener->next = 0; return listener; }
        bool removeListener(ItemListener *) { listeners = 0; return true; }

        class QContactProxy
        {
            friend class CacheItem;

            const QContact *contact;
            bool destroy;

            QContactProxy(const QByteArray &contactData)
                : contact(new QContact)
                , destroy(true)
            {
                QDataStream is(contactData);
                is >> const_cast<QContact&>(*contact);
            }

            QContactProxy(const QContact *contact)
                : contact(contact)
                , destroy(false)
            {
            }

        public:
            ~QContactProxy()
            {
                if (destroy) {
                    delete contact;
                }
            }

            QContactId id() const { return contact->id(); }

            template<typename DetailType>
            DetailType detail() const { return contact->detail<DetailType>(); }

            template<typename DetailType>
            QList<DetailType> details() const { return contact->details<DetailType>(); }

            QList<QContactDetail> details() const { return contact->details(); }

            operator const QContact& () const { return *contact; }
        };

        const QContactProxy getContact() const
        {
            if (instantiatedContact) {
                return QContactProxy(instantiatedContact.data());
            }
            return QContactProxy(contactData);
        }

        void setContact(const QContact &contact)
        {
            if (instantiatedContact) {
                *instantiatedContact = contact;
            } else {
                QDataStream os(&contactData, QIODevice::WriteOnly);
                os << contact;
            }
        }

        QContact *instantiateContact()
        {
            // Ensure that this cacheitem contains an instantiated QContact
            if (!instantiatedContact) {
                instantiatedContact.reset(new QContact);

                QDataStream is(contactData);
                is >> const_cast<QContact&>(*instantiatedContact);

                contactData.clear();
            }
            return instantiatedContact.data();
        }

        void uninstantiateContact()
        {
            if (instantiatedContact) {
                // Release this instantiation
                QDataStream os(&contactData, QIODevice::WriteOnly);
                os << *instantiatedContact;

                instantiatedContact.reset();
            }
        }

        quint32 iid;
        quint64 statusFlags;
        ContactState contactState;
        ItemListener *listeners;
        QString nameGroup;
        QString displayLabel;
        QByteArray contactData;
        QScopedPointer<QContact> instantiatedContact;
        QScopedPointer<ItemData> itemData;
    };

    class ListModel : public QAbstractListModel
    {
    public:
        ListModel(QObject *parent = 0) : QAbstractListModel(parent) {}
        virtual ~ListModel() {}

        virtual void sourceAboutToRemoveItems(int begin, int end) = 0;
        virtual void sourceItemsRemoved() = 0;

        virtual void sourceAboutToInsertItems(int begin, int end) = 0;
        virtual void sourceItemsInserted(int begin, int end) = 0;

        virtual void sourceDataChanged(int begin, int end) = 0;

        virtual void sourceItemsChanged() = 0;

        virtual void makePopulated() = 0;
        virtual void updateDisplayLabelOrder() = 0;
        virtual void updateSortProperty() = 0;
        virtual void updateGroupProperty() = 0;
    };

    struct ResolveListener
    {
        virtual ~ResolveListener() {}

        virtual void addressResolved(const QString &, const QString &, CacheItem *item) = 0;
    };

    struct ChangeListener
    {
        virtual ~ChangeListener() {}

        virtual void itemUpdated(CacheItem *item) = 0;
        virtual void itemAboutToBeRemoved(CacheItem *item) = 0;
    };

    static SeasideCache *instance();

    static QContactId apiId(const QContact &contact);
    static QContactId apiId(quint32 iid);

    static bool validId(const QContactId &id);

    static quint32 internalId(const QContact &contact);
    static quint32 internalId(const QContactId &id);

    SeasideCache();
    ~SeasideCache();

    static void registerModel(ListModel *model, FilterType type, FetchDataType requiredTypes = FetchNone, FetchDataType extraTypes = FetchNone);
    static void unregisterModel(ListModel *model);

    static void registerUser(QObject *user);
    static void unregisterUser(QObject *user);

    static void registerChangeListener(ChangeListener *listener);
    static void unregisterChangeListener(ChangeListener *listener);

    static void unregisterResolveListener(ResolveListener *listener);

    static DisplayLabelOrder displayLabelOrder();
    static QString sortProperty();
    static QString groupProperty();

    static int contactId(const QContact &contact);

    static CacheItem *existingItem(const QContactId &id);
    static CacheItem *existingItem(quint32 iid);
    static CacheItem *itemById(const QContactId &id, bool requireComplete = true);
    static CacheItem *itemById(int id, bool requireComplete = true);
    static QContactId selfContactId();
    static QContact contactById(const QContactId &id);
    static QString nameGroup(const CacheItem *cacheItem);
    static QString determineNameGroup(const CacheItem *cacheItem);
    static QStringList allNameGroups();

    static void ensureCompletion(CacheItem *cacheItem);
    static void refreshContact(CacheItem *cacheItem);

    static CacheItem *itemByPhoneNumber(const QString &number, bool requireComplete = true);
    static CacheItem *itemByEmailAddress(const QString &email, bool requireComplete = true);
    static CacheItem *itemByOnlineAccount(const QString &localUid, const QString &remoteUid, bool requireComplete = true);

    static CacheItem *resolvePhoneNumber(ResolveListener *listener, const QString &msisdn, bool requireComplete = true);
    static CacheItem *resolveEmailAddress(ResolveListener *listener, const QString &email, bool requireComplete = true);
    static CacheItem *resolveOnlineAccount(ResolveListener *listener, const QString &localUid, const QString &remoteUid, bool requireComplete = true);

    static bool saveContact(const QContact &contact);
    static void removeContact(const QContact &contact);

    static void aggregateContacts(const QContact &contact1, const QContact &contact2);
    static void disaggregateContacts(const QContact &contact1, const QContact &contact2);

    static void fetchConstituents(const QContact &contact);

    static void fetchMergeCandidates(const QContact &contact);

    static const QList<quint32> *contacts(FilterType filterType);
    static bool isPopulated(FilterType filterType);

    static QString generateDisplayLabel(const QContact &contact, DisplayLabelOrder order = FirstNameFirst);
    static QString generateDisplayLabelFromNonNameDetails(const QContact &contact);
    static QUrl filteredAvatarUrl(const QContact &contact, const QStringList &metadataFragments = QStringList());

    static QString normalizePhoneNumber(const QString &input, bool validate = false);
    static QString minimizePhoneNumber(const QString &input, bool validate = false);

    void populate(FilterType filterType);
    void insert(FilterType filterType, int index, const QList<quint32> &ids);
    void remove(FilterType filterType, int index, int count);

    static int importContacts(const QString &path);
    static QString exportContacts();

    void setFirstName(FilterType filterType, int index, const QString &name);

    void reset();

    static QList<quint32> getContactsForFilterType(FilterType filterType);

    QList<quint32> m_contacts[FilterTypesCount];
    ListModel *m_models[FilterTypesCount];
    bool m_populated[FilterTypesCount];

    QList<CacheItem *> m_cache;
    QHash<quint32, int> m_cacheIndices;

    static SeasideCache *instancePtr;
    static QStringList allContactNameGroups;

    quint32 idAt(int index) const;
};


#endif
