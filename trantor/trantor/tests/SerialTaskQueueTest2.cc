#include <trantor/utils/SerialTaskQueue.h>
#include <iostream>
#include <atomic>
#include <future>
#ifndef _WIN32
#include <unistd.h>
#endif

int main()
{
    std::atomic<uint64_t> counter;
    counter = 0;
    std::promise<int> pro;
    auto ft = pro.get_future();
    trantor::SerialTaskQueue queue("");
    queue.runTaskInQueue([&counter, &pro, &queue]() {
        for (int i = 0; i < 10000; ++i)
        {
            queue.runTaskInQueue([&counter, &pro]() {
                ++counter;
                if (counter.load() == 110000)
                    pro.set_value(1);
            });
        }
    });
    for (int i = 0; i < 10; ++i)
    {
        std::thread([&counter, &queue, &pro]() {
            for (int i = 0; i < 10000; ++i)
            {
                queue.runTaskInQueue([&counter, &pro]() {
                    ++counter;
                    if (counter.load() == 110000)
                        pro.set_value(1);
                });
            }
        }).detach();
    }

    ft.get();
    std::cout << "counter=" << counter.load() << std::endl;
}