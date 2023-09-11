#pragma once

#include <iostream>

#include "cache.hpp"

#include <unordered_map>
#include <deque>
#include <chrono>

namespace cache {

    using datetime_type = int64_t;
    using freq_type = int8_t;

    const freq_type kS3FifoMaxFreq = 3;

    namespace impl {
        template<class Value>
        struct S3FifoNode {
            Value value;

            datetime_type expire_time;
            freq_type freq;

            S3FifoNode(const Value &v, datetime_type dt, freq_type fr) {
                value = v;
                expire_time = dt;
                freq = fr;
            }

            bool Expired(datetime_type current_time) const {
                return expire_time > current_time;
            }
        };

//        template<class Key>
//        struct S3DequeItem {
//            Key key;
//            freq_type freq;
//
//            S3DequeItem(const Key &k, freq_type fr) {
//                key = k;
//                freq = fr;
//            }
//        };
    };

    template<class Key, class Value>
    class S3FifoCache : public Cache<Key, Value> {

    public:
        using TableNode = impl::S3FifoNode<Value>;
        using TableNodePtr = std::shared_ptr<TableNode>;
        using DequeItem = TableNodePtr;

        S3FifoCache(size_t cache_size,
                    datetime_type ttl_sec);

        std::optional<Value> Get(const Key &key) override;

        bool Has(const Key &key) const override;

        void Delete(const Key &key) override;

        virtual Value DoGetByKey(const Key &key) const override = 0;

    private:
        // implementation details -- main structs are here
        std::deque<DequeItem> _small_fifo{};
        std::deque<DequeItem> _main_fifo{};
        std::deque<DequeItem> _ghost_fifo{};

        mutable std::unordered_map<Key, TableNodePtr> _table;

        // settings fields -- TODO: move to config ?
        size_t _small_to_main_thr{1};
        double _small_fifo_size_ratio{0.1};
        datetime_type _ttl_sec;
        size_t _small_fifo_size;
        size_t _main_fifo_size;
        size_t _cache_size_limit;

        // statistics fields
        mutable size_t _current_size{0};
        mutable size_t _put_count{0};
        mutable size_t _hit_count{0};
        mutable size_t _hit_ghost_count{0};
        mutable size_t _miss_count{0};
        mutable size_t _removed_count{0};


        /// impl-details methods
        datetime_type CurrentTime() const {
            const auto c = std::clock();
            return c;
        }

        inline void InsertS(TableNodePtr node_ptr) {
            _small_fifo.push_front(node_ptr);
        }

        inline void InsertM(TableNodePtr node_ptr) {
            node_ptr->freq = 0;
            _main_fifo.push_front(node_ptr);
        }

        bool GhostIsFull() {
            return _ghost_fifo.size() >= _main_fifo_size;
        }

        void InsertG(TableNodePtr node_ptr) {
            if (GhostIsFull()) {
                const auto item_ptr = _ghost_fifo.back();
                _ghost_fifo.pop_back();
                if (item_ptr->freq < 0) {
                    RemoveItem(item_ptr->key);
                }
            }

            // node.value == nullptr ?
            node_ptr->freq = -1;
            _ghost_fifo.push_front(node_ptr);
        }

        void EnsureFree() {
            while (_small_fifo.size() + _main_fifo.size() >= _cache_size_limit) {
                if (_main_fifo.size() >= _main_fifo_size || _small_fifo.size() == 0) {
                    printf("evicting M\n");
                    EvictM();
                } else {
                    printf("evicting S\n");
                    EvictS();
                }
            }
        }

        void EvictM() {
            while (!_main_fifo.empty()) {
                auto tail = _main_fifo.back();
                _main_fifo.pop_back();
                if (tail->freq > 0) {
                    --tail->freq;
                    _main_fifo.push_front(tail);
                } else {
                    RemoveItem(tail->key);
                }
            }
        }

        void EvictS() {
            while (!_small_fifo.empty()) {
                auto tail = _small_fifo.back();
                _small_fifo.pop_back();
                if (tail->freq > 0) {
                    InsertM(tail);
                } else {
                    InsertG(tail);
                }
            }
        }

        void RemoveItem(const Key &key) {
            ++_removed_count;

            _table.erase(key);
        }
    };

    template<class Key, class Value>
    S3FifoCache<Key, Value>::S3FifoCache(size_t cache_size, datetime_type ttl_sec) {
        _cache_size_limit = cache_size;

        _small_fifo_size = round(cache_size * _small_fifo_size_ratio);
        _main_fifo_size = cache_size - _small_fifo_size;
        _ttl_sec = ttl_sec;
    }

    template<class Key, class Value>
    std::optional<Value> S3FifoCache<Key, Value>::Get(const Key &key) {
        auto it = _table.find(key);
        if (it == _table.end()) {
            ++_miss_count;
            TableNode node{DoGetByKey(key), CurrentTime() + _ttl_sec, 0};
            auto node_ptr = std::make_shared<TableNode>(std::move(node));
            auto [emplaced_it, inserted] = _table.try_emplace(key, std::move(node_ptr));

            if (!inserted) { /* OOPS, some strange shit happened here */ }

            EnsureFree();
            InsertS(emplaced_it->second);
            return emplaced_it->second->value;
        }

        if (it->second->freq == -1) {
            ++_hit_ghost_count;
            ++_miss_count;
            // LOG_DEBUG() << "ghost queue hitted:";

            it->second->value = DoGetByKey(key);
            it->second->freq = 0; // not sure if we really need this here

            EnsureFree();
            InsertM(it->second);

            return it->second->value;
        }

        ++_hit_count;
        if (it->second->freq != kS3FifoMaxFreq) {
            // std::min may be better? dunno, try to bench it
            ++it->second->freq;
        }

        return it->second->value;
    }

    template<class Key, class Value>
    bool S3FifoCache<Key, Value>::Has(const Key &key) const {
        return _table.find(key) != _table.end();
    }

    template<class Key, class Value>
    void S3FifoCache<Key, Value>::Delete(const Key &key) {

    }
} //