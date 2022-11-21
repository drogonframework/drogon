#include <drogon/drogon_test.h>
#include <drogon/CacheMap.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/net/EventLoopThread.h>

#include <chrono>

using namespace drogon;
using namespace std::chrono_literals;

DROGON_TEST(CacheMapTest)
{
    trantor::EventLoopThread loopThread;
    loopThread.run();
    drogon::CacheMap<std::string, std::string> cache(loopThread.getLoop(),
                                                     0.1f,
                                                     4,
                                                     30);

    for (size_t i = 1; i < 40; i++)
        cache.insert(std::to_string(i), "a", i);
    cache.insert("bla", "");
    cache.insert("zzz", "-");
    std::this_thread::sleep_for(3s);
    CHECK(cache.find("0") == false);  // doesn't exist
    CHECK(cache.find("1") == false);  // timeout
    CHECK(cache.find("15") == true);
    CHECK(cache.find("bla") == true);

    cache.erase("30");
    CHECK(cache.find("30") == false);

    cache.modify("bla", [](std::string& s) { s = "asd"; });
    CHECK(cache["bla"] == "asd");

    std::string content;
    cache.findAndFetch("zzz", content);
    CHECK(content == "-");
}
