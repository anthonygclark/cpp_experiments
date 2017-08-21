#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <new>
#include <type_traits>
#include <utility>
#include <cstddef>

/**
 * @brief
 * @tparam T
 * @tparam N
 */
template<typename T, std::size_t N>
class linear_object_storage
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
    /* alias */
    using value_type = T;

    /* Required for std::allocator interop */
    template <class _Up>
        struct rebind { 
            using other = linear_object_storage<_Up, N>;
        };

    linear_object_storage() noexcept :
        m_slots_used{0},
        m_high_water{0}
    {
        clear();
    }

    linear_object_storage(linear_object_storage const &) = default;
    linear_object_storage(linear_object_storage &&) = default;
    linear_object_storage& operator=(linear_object_storage &&) = default;
    linear_object_storage& operator=(linear_object_storage const &) = delete;

    ~linear_object_storage() {
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

        if (slot_range_start_ == m_use_map_mark.cend() || (slot_range_start + num > N))
            throw std::bad_alloc{};

        /* alloc from storage */
        auto * ret = (new(storage + m_slots_used) T[num]);
        auto * ret_ = ret;

        /* store the pointers */
        for (std::size_t i = 0; i < num; ++i)
        {
            auto t = slot_range_start + i;
            m_use_map_mark[t] = true;
            m_use_map[t] = ret_++;
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

        if (found == m_use_map.cend() || (slot_start + num > N))
            throw std::bad_alloc{};

        for (std::size_t i = 0; i < num; ++i)
        {
            auto t = slot_start + i;

            /* Destruct */
            auto * obj = m_use_map[t];
            obj->~T();
            obj = nullptr;

            /* Mark it unused */
            m_use_map_mark[t] = false;
            --m_slots_used;
        }
    }
};

