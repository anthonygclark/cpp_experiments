#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <chrono>
#include <type_traits>
#include <vector>

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



template<typename T, std::size_t N, typename Lock>
class object_cache
{
private:
    /* Lock */
    Lock m_lock;
    /* How many m_use_map slots are used */
    std::size_t m_slots_used;
    /* Max slots ever used */
    std::size_t m_high_water;
    /* Index/Slot availability */
    std::array<bool, N> m_use_map_mark;
    /* Allocated pointers (mostly for deallocating) */
    std::array<T *, N> m_use_map;

    typename std::aligned_storage<sizeof(T), alignof(T)>::type storage[N];

    void clear()
    {
        for (auto & i : m_use_map_mark) i = false;
        for (auto & i : m_use_map) i = nullptr;
    }

public:
    object_cache() noexcept :
        m_slots_used{0},
        m_high_water{0}
    {
        clear();
    }

    ~object_cache() { clear(); }

    T * allocate(std::size_t num = 1)
    {
        assert(num == 1 && "It is only possible to allocate a single object per allocate call");

        std::lock_guard<Lock> lock{m_lock};

        /* Find the next index/slot available */
        auto next_unused_slot = std::find(m_use_map_mark.cbegin(), m_use_map_mark.cend(), false);
        auto next_avail_slot = std::distance(m_use_map_mark.cbegin(), next_unused_slot);
        
        if (next_avail_slot == N)
            throw std::bad_alloc();

        /* mark it used */
        m_use_map_mark[next_avail_slot] = true;

        /* alloc from storage */
        auto * t = static_cast<T *>(new(storage + m_slots_used) T);

        /* store the pointer */
        m_use_map[next_avail_slot] = t;

        ++m_slots_used;

        if (m_slots_used > m_high_water)
            m_high_water = m_slots_used;

        return t;
    }

    void deallocate(T * obj, std::size_t num = 1) noexcept
    {
        assert(num == 1 && "It is only possible to deallocate a single object per deallocate call");
 
        std::lock_guard<Lock> lock{m_lock};

        auto found = std::find(m_use_map.cbegin(), m_use_map.cend(), obj);
        auto found_slot = std::distance(m_use_map.cbegin(), found);

        m_use_map_mark[found_slot] = false;
        m_use_map[found_slot] = nullptr;
        --m_slots_used;
    }
};


/////////////////////////////////////////////////
///////////////// BENCHMARK /////////////////////
/////////////////////////////////////////////////

#include <random>

//#define BENCH_SHORT_ALLOC

#ifdef BENCH_SHORT_ALLOC
#include "short_alloc.hh"
#endif

struct foo 
{
#if 0
    int x;
    float f;
    double d;
#else
    char data[4096];
#endif
};

template<typename T>
void shuffle_container(T & container)
{
    std::random_device rd;
    std::mt19937 g{rd()};
    
    std::shuffle(container.begin(), container.end(), g);
}

int main(void)
{
    object_cache<foo, 100, std::mutex> alloc;
    
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
            shuffle_container(indices);
            shuffle_container(indices2);
            for (auto & j : indices) { auto * t = alloc.allocate(1); foos[j] = t; }
            for (auto & j : indices2) { alloc.deallocate(foos[j]); }
        }
        
        auto end = std::chrono::steady_clock::now();
        auto res1 = std::chrono::duration <double, std::milli> (end-start).count();
        
        std::cout << "object_cache took " << (res1) << std::endl;
    }
    
    ///////////////////
    {
        foo * foos[100] = {nullptr};
        auto start = std::chrono::steady_clock::now();
        
        for (int i = 0 ; i < 5000; ++i) {
            shuffle_container(indices);
            shuffle_container(indices2);
            for (auto & j : indices) { foo * t = reinterpret_cast<foo *>(std::calloc(1, sizeof(foo))); foos[j] = t; }
            for (auto & j : indices2) { std::free(foos[j]); }
        }
        
        auto end = std::chrono::steady_clock::now();
        auto res2 = std::chrono::duration <double, std::milli> (end-start).count();
        
        std::cout << "malloc took " << (res2) << std::endl;
    }
    
    ///////////////////
#ifdef BENCH_SHORT_ALLOC
    {
        foo * foos[100] = {nullptr};
        
        auto start = std::chrono::steady_clock::now();
        
        arena<(sizeof(foo) * 100), alignof(foo)> ar{};
        short_alloc<foo, (sizeof(foo) * 100), alignof(foo)> sa{ar};
        
        for (int i = 0 ; i < 5000; ++i) {
            shuffle_container(indices);
            shuffle_container(indices2);
            for (auto & j : indices) { auto * t = sa.allocate(1); foos[j] = t; }
            for (auto & j : indices2) { sa.deallocate(foos[j], 1); }
        }
        
        auto end = std::chrono::steady_clock::now();
        auto res3 = std::chrono::duration <double, std::milli> (end-start).count();
        
        std::cout << "short_alloc took " << (res3 / 5000.0f) << std::endl;
    }
#endif

}
