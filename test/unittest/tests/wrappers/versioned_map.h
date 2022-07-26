/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#ifndef WIREDTIGER_VERSIONED_MAP_H
#define WIREDTIGER_VERSIONED_MAP_H

#include <optional>
#include <map>
#include <memory>
#include <string>
#include "wiredtiger.h"
#include "../utils.h"
#include "cursor_wrapper.h"
#include "transaction_wrapper.h"

template <class Key, class T> class VersionedMap {
    public:
    typedef Key key_map;
    typedef T mapped_type;
    typedef std::pair<Key, T> value_type;
    typedef uint64_t size_type;

    VersionedMap(WT_SESSION *session, std::string &tableName);

    T get(const Key &key) const;
    T getTransactionWrapped(
      const Key &key, const std::string &config, std::optional<uint64_t> timeStamp) const;

    void set(const Key &key, const T &value);
    void setTransactionWrapped(const Key &key, const T &value, const std::string &config);

    std::string const &getTableName();
    WT_SESSION *getSession();

    class Iterator {
        public:
        explicit Iterator(VersionedMap &map);
        ~Iterator() = default;

        value_type get() const;
        Iterator &next();
        Iterator &prev();
        bool isOk();

        private:
        std::shared_ptr<CursorWrapper> _cursor;
        int _wtRet;
    };

    Iterator begin();

    // Methods that are the same or similar to those in std::map
    [[nodiscard]] size_type size() const;
    [[nodiscard]] size_type size_transaction_wrapped(const std::string &config) const;

    private:
    WT_SESSION *_session; // This class does not own this pointer so should not free it.
    std::string _tableName;
};

template <class Key, class T>
VersionedMap<Key, T>::Iterator::Iterator(VersionedMap &map)
    : _wtRet(0), _cursor(std::make_shared<CursorWrapper>(map.getSession(), map.getTableName()))
{
    _cursor->reset();
    _wtRet = _cursor->next();
}

template <class Key, class T>
typename VersionedMap<Key, T>::value_type
VersionedMap<Key, T>::Iterator::get() const
{
    return { _cursor->getKey(), _cursor->getValue() };
};

template <class Key, class T>
typename VersionedMap<Key, T>::Iterator &
VersionedMap<Key, T>::Iterator::next()
{
    _wtRet = _cursor->next();
    return *this;
};

template <class Key, class T>
typename VersionedMap<Key, T>::Iterator &
VersionedMap<Key, T>::Iterator::prev()
{
    _wtRet = _cursor->prev();
    return *this;
};

template <class Key, class T>
bool
VersionedMap<Key, T>::Iterator::isOk()
{
    return _wtRet == 0;
};

template <class Key, class T>
VersionedMap<Key, T>::VersionedMap(WT_SESSION *session, std::string &tableName)
    : _session(session), _tableName(tableName)
{
}

template <class Key, class T>
std::string const &
VersionedMap<Key, T>::getTableName()
{
    return _tableName;
};

template <class Key, class T>
WT_SESSION *
VersionedMap<Key, T>::getSession()
{
    return _session;
};

template <class Key, class T>
typename VersionedMap<Key, T>::Iterator
VersionedMap<Key, T>::begin()
{
    return Iterator(*this);
};

template <class Key, class T>
T
VersionedMap<Key, T>::get(const Key &key) const
{
    CursorWrapper cursorWrapper(_session, _tableName);

    cursorWrapper.setKey(key);
    cursorWrapper.search();
    std::string value = cursorWrapper.getValue();
    cursorWrapper.reset();

    return value;
}

template <class Key, class T>
T
VersionedMap<Key, T>::getTransactionWrapped(
  const Key &key, const std::string &config, std::optional<uint64_t> timeStamp) const
{
    TransactionWrapper transactionWrapper(_session, config);
    if (timeStamp) {
        uint64_t ts = timeStamp.value();
        int ret = _session->timestamp_transaction_uint(_session, WT_TS_TXN_TYPE_READ, ts);
        utils::throwIfNonZero(ret);
    }
    return get(key);
}

template <class Key, class T>
void
VersionedMap<Key, T>::set(const Key &key, const T &value)
{
    CursorWrapper cursorWrapper(_session, _tableName);
    cursorWrapper.setKey(key);
    cursorWrapper.setValue(value);
    cursorWrapper.insert();
    cursorWrapper.reset();
}

template <class Key, class T>
void
VersionedMap<Key, T>::setTransactionWrapped(
  const Key &key, const T &value, const std::string &config)
{
    TransactionWrapper transactionWrapper(_session, config);
    set(key, value);
    transactionWrapper.commit("");
}

template <class Key, class T>
typename VersionedMap<Key, T>::size_type
VersionedMap<Key, T>::size() const
{
    CursorWrapper cursorWrapper(_session, _tableName);

    int ret = cursorWrapper.next();
    utils::throwIfNonZero(ret);
    size_type numValues = 0;
    while (ret == 0) {
        numValues++;
        ret = cursorWrapper.next();
    }
    utils::throwIfNotEqual(ret, WT_NOTFOUND); // Check for end-of-table.

    return numValues;
}

template <class Key, class T>
typename VersionedMap<Key, T>::size_type
VersionedMap<Key, T>::size_transaction_wrapped(const std::string &config) const
{
    TransactionWrapper transactionWrapper(_session, config);
    return size();
}

#endif // WIREDTIGER_VERSIONED_MAP_H
