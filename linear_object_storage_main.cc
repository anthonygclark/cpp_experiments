#include <random>
#include <vector>
#include <chrono>
#include <iostream>

#include "linear_object_storage.hh"

/* config */
#define BENCH_SHORT_ALLOC

#ifdef BENCH_SHORT_ALLOC
#include "short_alloc.hh"
#endif

struct foo
{
#if 0
    char data[4096];
#else
    int x[110];
    float y[400];
    uint8_t g,q,e;
    uint_least16_t s;
#endif
};

////////////////////////////////// SIMPLE TEST1
#if 0
int main()
{
    using dest = linear_object_storage<foo, 50>;
    using my_vec = std::vector<foo, dest>;

    dest alloc;
    my_vec v{alloc};

    for (size_t i = 0; i < 10; i++)
        v.emplace_back();
}

/////////////////////////////////// FULL BENCHMARK
#else

template<typename T>
void shuffle_container(T & container)
{
    std::random_device rd;
    std::mt19937 g{rd()};

    std::shuffle(container.begin(), container.end(), g);
}

#define TEST1(NAME, NUM, ALLOC, DEALLOC)                                              \
    do {                                                                             \
        auto start = std::chrono::steady_clock::now();                               \
                                                                                     \
        foo * foos[100] = {nullptr};                                                 \
                                                                                     \
        for (std::size_t i = 0 ; i < NUM; ++i)                                       \
        {                                                                            \
            shuffle_container(indices);                                              \
            shuffle_container(indices2);                                             \
            for (auto & j : indices) { foos[j] = (ALLOC); }                          \
            for (auto & j : indices2) { (DEALLOC); }                                 \
        }                                                                            \
                                                                                     \
        auto end = std::chrono::steady_clock::now();                                 \
        auto res1 = std::chrono::duration <double, std::milli>(end-start).count();   \
                                                                                     \
        std::cout << NAME << " took " << (res1) << "ms" << std::endl;                \
                                                                                     \
    } while (0)


int main(void)
{
    /* Setup storage and indicies */
    std::vector<std::size_t> indices;
    std::vector<std::size_t> indices2;
    indices.reserve(100);
    indices2.reserve(100);

    for (std::size_t i = 0; i < 100; ++i)
        indices.push_back(i);

    indices2 = indices;

    /***** Allocators */
    linear_object_storage<foo, 100> alloc;

#ifdef BENCH_SHORT_ALLOC
    arena<(sizeof(foo) * 100), alignof(foo)> ar{};
    short_alloc<foo, (sizeof(foo) * 100), alignof(foo)> sa{ar};
#endif
    /*****************/

    std::size_t test1_iters = 5000;

    /* Tests */
    TEST1("linear_object_storage", test1_iters,
         (alloc.allocate(1)),
         (alloc.deallocate(foos[j], 1))
         );

    TEST1("malloc", test1_iters,
         (reinterpret_cast<foo *>(std::malloc(sizeof(foo)))),
         (std::free(foos[j]))
        );

    TEST1("new", test1_iters,
         (new foo{}),
         (delete foos[j])
        );

#ifdef BENCH_SHORT_ALLOC
    TEST1("short_alloc", test1_iters,
         (sa.allocate(1)),
         (sa.deallocate(foos[j], 1))
        );
#endif

}
#endif
