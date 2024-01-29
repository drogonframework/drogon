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
        "keys id_*");
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

    // 9. Test sync
    try
    {
        auto res = redisClient->execCommandSync<std::string>(
            [](const RedisResult &result) { return result.asString(); },
            "set %s %s",
            "sync_key",
            "sync_value");
        MANDATE(res == "OK");
    }
    catch (const RedisException &err)
    {
        MANDATE(err.what());
    }

    try
    {
        auto [isNull, str] =
            redisClient->execCommandSync<std::pair<bool, std::string>>(
                [](const RedisResult &result) -> std::pair<bool, std::string> {
                    if (result.isNil())
                    {
                        return {true, ""};
                    }
                    return {false, result.asString()};
                },
                "get %s",
                "sync_key");
        MANDATE(isNull == false);
        MANDATE(str == "sync_value");
    }
    catch (const RedisException &err)
    {
        MANDATE(err.what());
    }

    // 10. Test sync redis exception
    try
    {
        auto [isNull, str] =
            redisClient->execCommandSync<std::pair<bool, std::string>>(
                [](const RedisResult &result) -> std::pair<bool, std::string> {
                    if (result.isNil())
                    {
                        return {true, ""};
                    }
                    return {false, result.asString()};
                },
                "get %s %s",
                "sync_key",
                "sync_key");
        MANDATE(false);
    }
    catch (const RedisException &err)
    {
        LOG_INFO << "Successfully catch sync error: " << err.what();
        MANDATE(err.code() == RedisErrorCode::kRedisError);
        SUCCESS();
    }

    // 11. Test sync process function exception
    try
    {
        auto value = redisClient->execCommandSync<std::string>(
            [](const RedisResult &result) {
                if (result.isNil())
                {
                    throw std::runtime_error("Key not exists");
                }
                return result.asString();
            },
            "get %s",
            "not_exists");
        MANDATE(false);
    }
    catch (const RedisException &err)
    {
        (void)err;
        MANDATE(false);
    }
    catch (const std::runtime_error &err)
    {
        MANDATE(std::string("Key not exists") == err.what());
        SUCCESS();
    }

    // 12. Test omit template parameter
    try
    {
        auto i = redisClient->execCommandSync(
            [](const RedisResult &r) { return r.asInteger(); },
            "del %s",
            "sync_key");
        MANDATE(i == 1);
    }
    catch (const RedisException &err)
    {
        MANDATE(err.what());
    }
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
