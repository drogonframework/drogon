#define DROGON_TEST_MAIN
#include <drogon/nosql/RedisClient.h>
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;
using namespace drogon::nosql;

RedisClientPtr redisClient;
DROGON_TEST(RedisTest)
{
    redisClient = drogon::nosql::RedisClient::newRedisClient(
        trantor::InetAddress("127.0.0.1", 6379), 1);
    REQUIRE(redisClient != nullptr);
    // std::this_thread::sleep_for(1s);
    redisClient->newTransactionAsync(
        [TEST_CTX](const RedisTransactionPtr &transPtr) {
            // 1
            transPtr->execCommandAsync(
                [TEST_CTX](const drogon::nosql::RedisResult &r) { SUCCESS(); },
                [TEST_CTX](const std::exception &err) { MANDATE(err.what()); },
                "ping");
            // 2
            transPtr->execute(
                [TEST_CTX](const drogon::nosql::RedisResult &r) { SUCCESS(); },
                [TEST_CTX](const std::exception &err) { MANDATE(err.what()); });
        });
    // 3
    redisClient->execCommandAsync(
        [TEST_CTX](const drogon::nosql::RedisResult &r) { SUCCESS(); },
        [TEST_CTX](const std::exception &err) { MANDATE(err.what()); },
        "set %s %s",
        "id_123",
        "drogon");
    // 4
    redisClient->execCommandAsync(
        [TEST_CTX](const drogon::nosql::RedisResult &r) {
            MANDATE(r.type() == RedisResultType::kArray);
            MANDATE(r.asArray().size() == 1UL);
        },
        [TEST_CTX](const std::exception &err) { MANDATE(err.what()); },
        "keys *");
    // 5
    redisClient->execCommandAsync(
        [TEST_CTX](const drogon::nosql::RedisResult &r) {
            MANDATE(r.asString() == "hello");
        },
        [TEST_CTX](const RedisException &err) { MANDATE(err.what()); },
        "echo %s",
        "hello");
    // 6
    redisClient->execCommandAsync(
        [TEST_CTX](const drogon::nosql::RedisResult &r) { SUCCESS(); },
        [TEST_CTX](const RedisException &err) { MANDATE(err.what()); },
        "flushall");
    // 7
    redisClient->execCommandAsync(

        [TEST_CTX](const drogon::nosql::RedisResult &r) {
            MANDATE(r.type() == RedisResultType::kNil);
        },
        [TEST_CTX](const RedisException &err) { MANDATE(err.what()); },
        "get %s",
        "xxxxx");

#ifdef __cpp_impl_coroutine
    auto coro_test = [TEST_CTX]() -> drogon::Task<> {
        // 8
        try
        {
            auto r = co_await redisClient->execCommandCoro("get %s", "haha");
            MANDATE(r.type() == RedisResultType::kNil);
        }
        catch (const RedisException &err)
        {
            FAULT(err.what());
        }
    };
    drogon::sync_wait(coro_test());
#endif
}

int main(int argc, char **argv)
{
#ifndef USE_REDIS
    LOG_DEBUG << "Drogon is built without Redis. No tests executed.";
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
