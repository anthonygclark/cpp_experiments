#include <type_traits>

namespace type_traits
{
    template <typename...>
    struct is_in_pack {
        static constexpr bool value = false;
    };
    
    template <typename F, typename S, typename... Next>
    struct is_in_pack<F, S, Next...> {
        static constexpr bool value = std::is_same<F, S>::value || is_in_pack<F, Next...>::value;
    };
}

template<typename... Ts>
void foo(Ts &&... ts)
{
    static_assert(type_traits::is_in_pack<int, Ts...>::value, "int must be in pack");
}

int main()
{
    foo("test", 5u, 0.1f);
}
