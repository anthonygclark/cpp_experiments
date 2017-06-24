#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <type_traits>
#include <vector>

template<typename T, std::size_t N>
class object_cache
{
private:
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
        m_slots_used = 0;
    }

public:
    using value_type = T;
    template <class _Up> struct rebind { using other = object_cache<_Up, N>; };

    object_cache() noexcept :
        m_slots_used{0},
        m_high_water{0}
    {
        clear();
    }

    object_cache(object_cache const &) = default;
    object_cache(object_cache &&) = default;
    object_cache& operator=(object_cache &&) = default;
    object_cache& operator=(object_cache const &) = delete;

    ~object_cache()
    {
        clear();
    }

    std::pair<std::size_t, std::size_t> get_info() const {
        return std::make_pair(m_slots_used, m_high_water);
    }

    T * allocate(std::size_t num)
    {
        /* Find group of num slots available */
        auto slot_range_start_ = std::search_n(m_use_map_mark.cbegin(), m_use_map_mark.cend(), num, false);
        auto slot_range_start = std::distance(m_use_map_mark.cbegin(), slot_range_start_);

        if (slot_range_start_ == m_use_map_mark.cend() || slot_range_start + num > N)
            throw std::bad_alloc();

        /* alloc from storage */
        auto * ret = (new(storage + m_slots_used) T[num]);
        auto * ret_ = ret;

        /* store the pointers */
        for (std::size_t i = 0; i < num; ++i) {
            m_use_map_mark[slot_range_start + i] = true;
            m_use_map[slot_range_start + i] = ret_++;
        }

        m_slots_used += num;

        if (m_slots_used > m_high_water)
            m_high_water = m_slots_used;

        return ret;
    }

    void deallocate(T * obj, std::size_t num)
    {
        /* Find obj in the use map */
        auto found = std::find(m_use_map.cbegin(), m_use_map.cend(), obj);
        auto slot_start = std::distance(m_use_map.cbegin(), found);

        if (found == m_use_map.cend() || slot_start + num > N)
            throw std::bad_alloc();

        for (std::size_t i = 0; i < num; ++i)
        {
            /* Destruct */
            auto obj = m_use_map[slot_start + i];
            obj->~T();
            obj = nullptr;

            /* Mark it unused */
            m_use_map_mark[slot_start + i] = false;
            --m_slots_used;
        }
    }
};


/////////////////////////////////////////////////
/////////////////////// MAIN ////////////////////
/////////////////////////////////////////////////

struct foo
{
#if 0
    int x;
#else
    float x;
    char data[1024];
    uint8_t z;
    int a;
#endif
};

////////////////////////////////// SIMPLE TEST
#if 0
int main()
{
    using cache = object_cache<foo, 50>;
    using my_vec = std::vector<foo, object_cache<foo, 50>>;

    cache alloc;
    my_vec v{alloc};

    for (size_t i = 0; i < 10; i++)
        v.emplace_back();
}

/////////////////////////////////// FULL BENCHMARK
#else

#include <random>
//#define BENCH_SHORT_ALLOC

#ifdef BENCH_SHORT_ALLOC
#include "short_alloc.hh"
#endif

template<typename T>
void shuffle_container(T & container)
{
    std::random_device rd;
    std::mt19937 g{rd()};

    std::shuffle(container.begin(), container.end(), g);
}

int main(void)
{
    object_cache<foo, 100> alloc;

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
            for (auto & j : indices2) { alloc.deallocate(foos[j], 1); }
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

        std::cout << "short_alloc took " << (res3) << std::endl;
    }
#endif

}
#endif
