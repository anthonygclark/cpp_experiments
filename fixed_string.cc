#include <map>
#include <unordered_map>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cassert>

/* config */
#define ALLOW_FIXED_STRING_STD_HASH

template<std::size_t N>
struct FixedString
{
    char m_data[N];
    std::size_t m_length;

    FixedString() noexcept :
        m_length(0)
    {
        std::memset(m_data, 0, N);
    }

    FixedString(const char * src, std::size_t len = 0) noexcept :
        FixedString()
    {
        assert(src);

        if (len == 0 || len > N)
            len = ::strnlen(src, N-1);

        m_length = len;

        std::memcpy(m_data, src, len);

        if (len == N && m_data[N-1] != '\0')
            m_data[N-1] = '\0';

        if (m_data[len] != '\0')
            m_data[len] = '\0';
    };

    const char * c_str() const
    {
        return m_data;
    }

    std::size_t length() const
    {
        return m_length;
    }

    std::size_t hash() const
    {
        constexpr static const long djb2_start = 0x1505;

        using concreate_data_type = typename std::remove_all_extents<decltype(std::declval<FixedString<N>>().m_data)>::type;
        using concreate_data_type_pointer = const concreate_data_type *;
        using concreate_length_type = decltype(std::declval<FixedString<N>>().length());

        concreate_data_type_pointer data_ptr = c_str();

        std::size_t h1 = djb2_start;

        /* djb2 hash algorithm (http://www.cse.yorku.ca/~oz/hash.html) */
        while (concreate_data_type c = *data_ptr++)
            h1 = ((h1 << 5) + h1) + c;

        typename std::hash<concreate_length_type>::result_type const h2 {
            std::hash<concreate_length_type>()(length())
        };

        return h1 ^ (h2 << 1);

    }

    template<std::size_t M>
    friend bool operator <(FixedString<N> const & lhs, FixedString<M> const & rhs)
    {
        if (N > M) return false;
        if (M > N) return true;

        if (M == N)
            return std::strncmp(lhs.m_data, rhs.m_data, N) < 0;
    }

    template<std::size_t M>
    friend bool operator >(FixedString<N> const & lhs, FixedString<M> const & rhs)
    {
        return !(lhs < rhs);
    }

    template<std::size_t M>
    friend bool operator ==(FixedString<N> const & lhs, FixedString<M> const & rhs)
    {
        if (N != M) return false;

        if (M == N)
            return std::strncmp(lhs.m_data, rhs.m_data, N) == 0;
    }
};

#ifdef ALLOW_FIXED_STRING_STD_HASH
namespace std
{
    template<std::size_t N>
    struct hash<FixedString<N>>
    {
        using argument_type = FixedString<N>;
        using result_type = std::size_t;

        result_type operator()(argument_type const & fs) const
        {
            return fs.hash();
        }
    };
}
#endif

int main()
{
    // as val
    std::unordered_map<int, FixedString<4>> umap;
    umap.emplace(1, "test2");

    for (auto & t : umap)
        std::printf("umap[%d] = '%s'\n", t.first, t.second.c_str());

    // as key
    std::map<FixedString<64>, int> map;
    map.emplace("test", 2);

    for (auto & t : map)
        std::printf("map[%s] = '%d'\n", t.first.c_str(), t.second);

    FixedString<64> h{"ascii!"};
    FixedString<3> hh{"ascii!"};

    std::printf("h: %s %zu, hh: %s %zu\n",
                h.c_str(), h.length(), hh.c_str(), hh.length());
}

