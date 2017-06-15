#include <iostream>
#include <type_traits>

#include <atomic>
#include <memory>
#include <mutex>

namespace Logging
{
    std::mutex logging_mutex;
    
    enum Levels
    {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
    };

    const char * LevelNames[] = {
        "DEBUG   ==> ", 
        "INFO    ==> ",
        "WARNING ==> ",
        "ERROR   ==> ",
    }; 

#ifndef CT_LOG_LEVEL
# define CT_LOG_LEVEL 99
#endif
    
constexpr int log_level = CT_LOG_LEVEL;

#undef CT_LOG_LEVEL

    template<enum Levels L, typename... Args>
        typename std::enable_if<(log_level >= L)>::type write(Args const & ... args)
        {
            std::lock_guard<decltype(logging_mutex)> lk{logging_mutex};
            
            std::cout << LevelNames[L];
            std::initializer_list<int> l { (std::cout << args << ' ', 0)... };
            std::cout << std::endl;
            (void)l;
        }

    template<enum Levels L, typename... Args>
        typename std::enable_if<(log_level < L)>::type write(Args const & ... args)
        { }
}

using namespace Logging;

int main(void)
{
    write<DEBUG>("anthony", 12.4, 12, "test");
    write<INFO>("anthony", 12.4, 12, "test");
    write<WARNING>("anthony", 12.4, 12, "test");
    write<ERROR>("anthony", 12.4, 12, "test");
}
