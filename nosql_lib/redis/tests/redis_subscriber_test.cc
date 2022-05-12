#define DROGON_TEST_MAIN
#include <drogon/nosql/RedisClient.h>
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;
using namespace drogon::nosql;
static std::atomic_int nMsgRecv{0};
static std::atomic_int nMsgSent{0};

RedisClientPtr redisClient;
DROGON_TEST(RedisSubscriberTest)
{
    redisClient = drogon::nosql::RedisClient::newRedisClient(
        trantor::InetAddress("127.0.0.1", 6379), 1);
    REQUIRE(redisClient != nullptr);

    auto subscriber = redisClient->newSubscriber();
    subscriber->subscribe(
        "test_sub", [](const std::string &channel, const std::string &message) {
            ++nMsgRecv;
            LOG_INFO << "Channel test_sub receive " << nMsgRecv << " messages";
        });
    std::this_thread::sleep_for(1s);

    auto fnPublish = [TEST_CTX](int i) {
        redisClient->execCommandAsync(
            [TEST_CTX](const drogon::nosql::RedisResult &r) {
                SUCCESS();
                ++nMsgSent;
            },
            [TEST_CTX](const std::exception &err) {
                MANDATE(err.what());
                LOG_ERROR << err.what();
                ++nMsgSent;
            },
            "publish %s %s%d",
            "test_sub",
            "drogon",
            i);
    };

    for (int i = 0; i < 10; ++i)
    {
        fnPublish(i);
    }

    while (nMsgSent < 10)
    {
        std::this_thread::sleep_for(100ms);
    }
    std::this_thread::sleep_for(1s);

    MANDATE(nMsgRecv == 10);

    subscriber->unsubscribe("test_sub");
    std::this_thread::sleep_for(1s);
    fnPublish(11);
    while (nMsgSent < 11)
    {
        std::this_thread::sleep_for(100ms);
    }
    MANDATE(nMsgRecv == 10);
}

int main(int argc, char **argv)
{
#ifndef USE_REDIS
    LOG_DEBUG << "Drogon is built without "
                 "Redis. No tests executed.";
    return 0;
#endif
    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();

    std::thread thr([&]() {
        p1.set_value();
        drogon::app().run();
    });

    f1.get();
    int testStatus = drogon::test::run(argc, argv);
    drogon::app().getLoop()->queueInLoop([]() { drogon::app().quit(); });
    thr.join();
    return testStatus;
}
