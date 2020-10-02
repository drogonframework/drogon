#include <drogon/CacheMap.h>
#include <drogon/utils/Utilities.h>
#include <iostream>
#include <string>
#include <thread>
#include <trantor/net/EventLoopThread.h>
#include <trantor/utils/Logger.h>
#include <chrono>
#include <thread>
using namespace std::chrono_literals;
int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::kTrace);
    trantor::EventLoopThread loopThread;
    loopThread.run();

    drogon::CacheMap<std::string, std::string> cache(loopThread.getLoop(),
                                                     0.1,
                                                     4,
                                                     30);
    std::this_thread::sleep_for(3s);
    for (int i = 0; i < 40; ++i)
    {
        cache.insert(drogon::utils::formattedString("aaa%d", i),
                     "hehe",
                     i,
                     [i]() {
                         std::cout << i << " cache item erased!" << std::endl;
                     });
    }
    cache.insert("1", "first", 20, [] {
        LOG_DEBUG << "first item in cache timeout,erase!";
    });
    cache.insert("2", "second", 5);
    cache.insert("3", "third", 5);
    std::thread thread1([&] {
        std::this_thread::sleep_for(1s);
        cache.erase("3");
        cache.insert("3", "THIRD");
        std::this_thread::sleep_for(10s);
        if (cache.find("1"))
        {
            LOG_DEBUG << "sleep 10,1 item:" << cache["1"];
        }
        else
        {
            LOG_DEBUG << "can't find 1 item";
        }
        std::this_thread::sleep_for(15s);
        if (cache.find("1"))
        {
            LOG_DEBUG << "sleep 15,1 item:" << cache["1"];
        }
        else
        {
            LOG_DEBUG << "can't find 1 item";
        }
        std::this_thread::sleep_for(20s);
        if (cache.find("1"))
        {
            LOG_DEBUG << "sleep 20,1 item:" << cache["1"];
        }
        else
        {
            LOG_DEBUG << "can't find 1 item";
        }
        std::this_thread::sleep_for(22s);
        if (cache.find("1"))
        {
            LOG_DEBUG << "sleep22,1 item:" << cache["1"];
        }
        else
        {
            LOG_DEBUG << "can't find 1 item";
        }

        if (cache.find("2"))
        {
            LOG_DEBUG << "sleep22,2 item:" << cache["2"];
        }
        else
        {
            LOG_DEBUG << "can't find 2 item";
        }

        if (cache.find("3"))
        {
            LOG_DEBUG << "sleep22,3 item:" << cache["3"];
        }
        else
        {
            LOG_DEBUG << "can't find 3 item";
        }
    });
    thread1.join();

    getchar();
}
