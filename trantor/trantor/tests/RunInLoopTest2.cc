#include <trantor/net/EventLoopThread.h>
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
    trantor::EventLoopThread loopThread;

    auto loop = loopThread.getLoop();
    loop->runInLoop([&counter, &pro, loop]() {
        for (int i = 0; i < 10000; ++i)
        {
            loop->queueInLoop([&counter, &pro]() {
                ++counter;
                if (counter.load() == 110000)
                    pro.set_value(1);
            });
        }
    });
    for (int i = 0; i < 10; ++i)
    {
        std::thread([&counter, loop, &pro]() {
            for (int i = 0; i < 10000; ++i)
            {
                loop->runInLoop([&counter, &pro]() {
                    ++counter;
                    if (counter.load() == 110000)
                        pro.set_value(1);
                });
            }
        }).detach();
    }
    loopThread.run();
    ft.get();
    std::cout << "counter=" << counter.load() << std::endl;
}
