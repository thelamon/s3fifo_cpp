#include <iostream>

#include "s3_cache.hpp"

class SquareCache : public cache::S3FifoCache<int64_t, int64_t> {
public:
    SquareCache(int64_t size) : cache::S3FifoCache<int64_t, int64_t>(size, 100) {}

    int64_t DoGetByKey(const int64_t &key) const override {
        return key * key;
    }
};

int main() {
    std::cout << "Hello, World!" << std::endl;

    SquareCache cache(10);

    cache.Get(1);
    cache.Get(2);
    cache.Get(3);
    cache.Get(4);
    cache.Get(5);
    cache.Get(6);
    cache.Get(7);
    cache.Get(8);
    cache.Get(9);
    cache.Get(10);
    assert(cache.Has(10));
    assert(cache.Has(1));
    cache.Get(11);
    assert(!cache.Has(1));
    cache.Get(2);
    cache.Get(12);
    assert(!cache.Has(3));
    return 0;
}
