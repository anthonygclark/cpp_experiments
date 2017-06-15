#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <thread>

std::sig_atomic_t stop = 0;
int max_index = 0;
std::mt19937_64 eng{std::random_device{}()};
std::uniform_int_distribution<> dist{1, 20}; // ms

/* stop function */
void onsig(int) { stop = 1; }

/* example object */
struct MyObject { /* just some data */ char c[64]; };

/* storage for PODs */
alignas(MyObject) std::uint8_t data[1024][sizeof(MyObject)];

/* simple locking queue */
template<typename T>
class Queue final
{
public:
    /**< Underlaying type */
    using type = T;
    /**< Queue type */
    using data_type = T;
    /**< Termination type */
    using terminator = std::nullptr_t;

private:
    /**< Underlaying queue */
    std::queue<data_type> data_queue;
    /**< Mutex (mutable since empty() is const */
    mutable std::mutex mutex;
    /**< Condition variable */
    std::condition_variable condition;

public:
    Queue() = default;

    data_type wait_and_pop()
    {
        std::unique_lock<std::mutex> lk{mutex};
        condition.wait(lk, [this] { return !data_queue.empty(); });
        data_type value = std::move(data_queue.front());
        data_queue.pop();
        return value;
    }

    data_type try_pop()
    {
        std::lock_guard<std::mutex> lk{mutex};
        if (data_queue.empty()) return nullptr;
        data_type value = std::move(data_queue.front());
        data_queue.pop();
        return value;
    }

    void push(T new_value)
    {
        { // scope
            /* Copy/move construct T TODO does this work ?!*/
            data_type data = new_value;
            std::lock_guard<std::mutex> lk{mutex};
            data_queue.push(std::move(data));
        }

        condition.notify_one();
    }

    void terminate() { push(terminator()); }
    int size() const { return data_queue.size(); }

private:
    /**
     * @brief Determines in the underlaying queue is empty
     * @return Empty status
     */
    bool empty() const
    {
        std::lock_guard<std::mutex> lk{mutex};
        return data_queue.empty();
    }

    /**
     * @brief Termination push
     * @param term Terminator type to tell the queue to stop
     */
    void push(terminator term)
    { 
        /* should be convertible to our data_type */
        data_type data = term;

        { // scope
            std::lock_guard<std::mutex> lk{mutex};
            data_queue.push(std::move(data));
        }

        condition.notify_one();
    }
};

/* alias */
using QUEUE = Queue<MyObject *>;

/* pops the queue randomly */
void read_thread(QUEUE & q)
{
    while (!stop)
    {
        for (int i = 0; i < 10; ++i)
        {
            MyObject * p = q.wait_and_pop();
            p->~MyObject();
            std::this_thread::sleep_for(std::chrono::milliseconds{dist(eng)});
        }
    }
}

/* writes the queue randomly */
void write_thread(QUEUE & q)
{
    while (!stop) 
    {
        for (int i = 0; i < 10; ++i)
        {
            q.push(new(data[i]) MyObject);
            auto s = q.size();
            if (max_index < s) max_index = s;
            std::cout << "WRITING : " << q.size() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds{dist(eng)});
        }
    }
}


int main(void)
{
    std::cout << sizeof(data) << std::endl;
    ::signal(SIGINT, onsig);

    QUEUE q;

    std::thread r{read_thread, std::ref(q)};
    std::thread w{write_thread, std::ref(q)};

    std::cout << "Press Control-C to quit\n";
    
    r.join();
    w.join();

    std::cout << "done\n";
    std::cout << "max index: " << max_index << std::endl;
}
