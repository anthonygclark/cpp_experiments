#include <iostream>
#include <chrono>
#include <vector>
#include <atomic>
#include <stdexcept>
#include <memory>
#include <algorithm>
#include <type_traits>
#include <cstring>
#include <cassert>
#include <thread>
#include <array>

// for benchmark
#include <random>

/* CONFIG */

//#define OBJECT_CACHE_USE_ATOMIC_SPIN_LOCK
//#define OBJECT_CACHE_USE_MUTEX
//#define OBJECT_CACHE_USE_ATOMIC_DATA_STRUCTURES
/* end CONFIG */

#ifdef OBJECT_CACHE_USE_MUTEX
#include <mutex>
#endif

#ifdef OBJECT_CACHE_USE_ATOMIC_SPIN_LOCK
#include <mutex>
#include <atomic>

/* Taken from "Concurrency in Action - Anthony Williams */
class spinlock_mutex
{
private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock() {
        while(flag.test_and_set(std::memory_order_acquire));
    }

    void unlock() {
        flag.clear(std::memory_order_release);
    }
};

#endif

/* Not compatible with stl containers */
template<typename T, std::size_t N>
class object_cache_alloc
{
private:

#ifdef OBJECT_CACHE_MULTITHREAD
    std::thread::id owning_thread_id;
#endif

#ifdef OBJECT_CACHE_USE_MUTEX
    std::mutex mutex;
#endif

#ifdef OBJECT_CACHE_USE_ATOMIC_SPIN_LOCK
    spinlock_mutex spinlock;
#endif

    int storage_active_count;

#ifdef OBJECT_CACHE_USE_ATOMIC_DATA_STRUCTURES
    std::array<std::atomic<int>, N> use_map_mark;
#else
    std::array<int, N> use_map_mark;
#endif

    std::array<T *, N> use_map;

    typename std::aligned_storage<sizeof(T), alignof(T)>::type storage[N];

    void clear()
    {
        for (auto & i : use_map_mark) i = 0;
        for (auto & i : use_map) i = nullptr;
    }

public:
    object_cache_alloc() noexcept :
#ifdef OBJECT_CACHE_MULTITHREAD
        owning_thread_id(std::this_thread::get_id()),
#endif
        storage_active_count(0)
    {
        clear();
    }

    ~object_cache_alloc() { clear(); }

    T * allocate(std::size_t num = 1)
    {
        assert(num == 1 && "It is only possible to allocate a single object per allocate call");

#ifdef OBJECT_CACHE_USE_MUTEX
        std::lock_guard<std::mutex> lock{mutex};
#endif

#ifdef OBJECT_CACHE_USE_ATOMIC_SPIN_LOCK
        std::lock_guard<spinlock_mutex> lock{spinlock};
#endif

        auto i = std::find(use_map_mark.cbegin(), use_map_mark.cend(), 0);
        auto next_avail_chunk = std::distance(use_map_mark.cbegin(), i);
        
        if (next_avail_chunk == N) {
            std::cout << "storage active count: " << storage_active_count << std::endl;
            throw std::bad_alloc();
        }

        use_map_mark[next_avail_chunk] = 1;

        auto * t = static_cast<T *>(new(storage + storage_active_count) T);
        use_map[next_avail_chunk] = t;

        storage_active_count++;

        return t;
    }

    void deallocate(T * obj, std::size_t num = 1) noexcept
    {
        assert(num == 1 && "It is only possible to deallocate a single object per deallocate call");

#ifdef OBJECT_CACHE_USE_MUTEX
        std::lock_guard<std::mutex> lock{mutex};
#endif

#ifdef OBJECT_CACHE_USE_ATOMIC_SPIN_LOCK
        std::lock_guard<spinlock_mutex> lock{spinlock};
#endif

        auto i = std::find(use_map.cbegin(), use_map.cend(), obj);
        auto chunk_location = std::distance(use_map.cbegin(), i);

        use_map_mark[chunk_location] = 0;
        use_map[chunk_location] = nullptr;
        storage_active_count--;
    }
};

//#define BENCH_SHORT_ALLOC

#ifdef BENCH_SHORT_ALLOC
#include "short_alloc.hh"
#endif

struct foo {
    int x;
    float f;
    double d;
    //char d1[64];
};

template<typename T>
void shuffle_t(T & container)
{
    std::random_device rd;
    std::mt19937 g{rd()};
    
    std::shuffle(container.begin(), container.end(), g);
}

int main(void)
{
    object_cache_alloc<foo, 100> alloc;
    
    std::vector<int> indices;
    std::vector<int> indices2;
    indices.reserve(100);
    indices2.reserve(100);
    
    for (int i = 0; i < 100; ++i)
        indices.push_back(i);
    
    indices2 = indices;

    ///////////////////
    {
        auto start = std::chrono::steady_clock::now();
        
        foo * foos[100] = {nullptr};
        
        for (int i = 0 ; i < 5000; ++i) {
            shuffle_t(indices);
            shuffle_t(indices2);
            for (auto & j : indices) { auto * t = alloc.allocate(1); foos[j] = t; }
            for (auto & j : indices2) { alloc.deallocate(foos[j]); }
        }
        
        auto end = std::chrono::steady_clock::now();
        auto res1 = std::chrono::duration <double, std::milli> (end-start).count();
        
        std::cout << "object_cache_alloc took " << (res1 / 5000.0f) << std::endl;
    }
    
    ///////////////////
    {
        foo * foos[100] = {nullptr};
        auto start = std::chrono::steady_clock::now();
        
        for (int i = 0 ; i < 5000; ++i) {
            shuffle_t(indices);
            shuffle_t(indices2);
            for (auto & j : indices) { foo * t = reinterpret_cast<foo *>(std::calloc(1, sizeof(foo))); foos[j] = t; }
            for (auto & j : indices2) { std::free(foos[j]); }
        }
        
        auto end = std::chrono::steady_clock::now();
        auto res2 = std::chrono::duration <double, std::milli> (end-start).count();
        
        std::cout << "malloc took " << (res2 / 5000.0f) << std::endl;
    }
    
    ///////////////////
#ifdef BENCH_SHORT_ALLOC
    {
        foo * foos[100] = {nullptr};
        
        auto start = std::chrono::steady_clock::now();
        
        arena<(sizeof(foo) * 100), alignof(foo)> ar{};
        short_alloc<foo, (sizeof(foo) * 100), alignof(foo)> sa{ar};
        
        for (int i = 0 ; i < 5000; ++i) {
            shuffle_t(indices);
            shuffle_t(indices2);
            for (auto & j : indices) { auto * t = sa.allocate(1); foos[j] = t; }
            for (auto & j : indices2) { sa.deallocate(foos[j], 1); }
        }
        
        auto end = std::chrono::steady_clock::now();
        auto res3 = std::chrono::duration <double, std::milli> (end-start).count();
        
        std::cout << "short_alloc took " << (res3 / 5000.0f) << std::endl;
    }
#endif

}
