#pragma once

template<class Key, class Value>
class Cache {

public:
    /**
     * const methods
     */
    virtual std::optional<Value> Get(const Key &key) = 0;

    virtual bool Has(const Key &key) const = 0;

    /**
     * modifying methods
     */
    virtual void Delete(const Key &key) = 0;

    virtual Value DoGetByKey(const Key& key) const = 0;
    /*
     * observability methods
     */
//     virtual bool IsEmpty() const = 0;
//
//     size_t Size() const;
};
