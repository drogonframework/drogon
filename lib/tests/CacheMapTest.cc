#include <drogon/CacheMap.h>
#include <drogon/utils/Utilities.h>
#include <iostream>
#include <string>
#include <thread>
#include <trantor/net/EventLoopThread.h>
#include <trantor/utils/Logger.h>
#include <unistd.h>

int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    trantor::EventLoopThread loopThread;
    loopThread.run();

    drogon::CacheMap<std::string, std::string> cache(loopThread.getLoop(), 0.1, 4, 30);
    sleep(3);
    for (int i = 0; i < 40; i++)
    {
        cache.insert(drogon::utils::formattedString("aaa%d", i), "hehe", i, [=]() {
            std::cout << i << " cache item erased!" << std::endl;
        });
    }
    cache.insert("1", "first", 20, [=] { LOG_DEBUG << "first item in cache timeout,erase!"; });
    cache.insert("2", "second", 5);
    cache.insert("3", "third", 5);
    std::thread thread1([&] {
        sleep(1);
        cache.erase("3");
        cache.insert("3", "THIRD");
        sleep(10);
        if (cache.find("1"))
        {
            LOG_DEBUG << "sleep 10,1 item:" << cache["1"];
        }
        else
        {
            LOG_DEBUG << "can't find 1 item";
        }
        sleep(15);
        if (cache.find("1"))
        {
            LOG_DEBUG << "sleep 15,1 item:" << cache["1"];
        }
        else
        {
            LOG_DEBUG << "can't find 1 item";
        }
        sleep(20);
        if (cache.find("1"))
        {
            LOG_DEBUG << "sleep 20,1 item:" << cache["1"];
        }
        else
        {
            LOG_DEBUG << "can't find 1 item";
        }
        sleep(22);
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
