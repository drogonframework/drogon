#include <drogon/nosql/RedisClient.h>
#include <drogon/drogon.h>
#include <iostream>
#include <thread>
#define RESET "\033[0m"
#define RED "\033[31m"   /* Red */
#define GREEN "\033[32m" /* Green */
using namespace std::chrono_literals;
using namespace drogon::nosql;
#ifdef __cpp_impl_coroutine
#define TEST_COUNT 8
#else
#define TEST_COUNT 7
#endif
std::promise<int> pro;
auto globalf = pro.get_future();
int counter = 0;
void addCount(int &count, std::promise<int> &pro)
{
    ++count;
    // LOG_DEBUG << count;
    if (count == TEST_COUNT)
    {
        pro.set_value(1);
    }
}
void testoutput(bool isGood, const std::string &testMessage)
{
    if (isGood)
    {
        std::cout << GREEN << counter + 1 << ".\t" << testMessage << "\t\tOK\n";
        std::cout << RESET;
        addCount(counter, pro);
    }
    else
    {
        std::cout << RED << testMessage << "\t\tBAD\n";
        std::cout << RESET;
        exit(1);
    }
}

void doTest(const RedisClientPtr &redisClient)
{
    // std::this_thread::sleep_for(1s);
    redisClient->newTransactionAsync([](const RedisTransactionPtr &transPtr) {
        // 1
        transPtr->execCommandAsync(
            [](const drogon::nosql::RedisResult &r) {
                testoutput(true, r.getStringForDisplaying());
            },
            [](const std::exception &err) { testoutput(false, err.what()); },
            "ping");
        // 2
        transPtr->execute(
            [](const drogon::nosql::RedisResult &r) {
                testoutput(true, r.getStringForDisplaying());
            },
            [](const std::exception &err) { testoutput(false, err.what()); });
    });
    // 3
    redisClient->execCommandAsync(
        [](const drogon::nosql::RedisResult &r) {
            testoutput(true, r.getStringForDisplaying());
        },
        [](const std::exception &err) { testoutput(false, err.what()); },
        "set %s %s",
        "id_123",
        "drogon");
    // 4
    redisClient->execCommandAsync(
        [](const drogon::nosql::RedisResult &r) {
            testoutput(r.type() == RedisResultType::kArray &&
                           r.asArray().size() == 1,
                       r.getStringForDisplaying());
        },
        [](const std::exception &err) { testoutput(false, err.what()); },
        "keys *");
    // 5
    redisClient->execCommandAsync(
        [](const drogon::nosql::RedisResult &r) {
            testoutput(r.asString() == "hello", r.getStringForDisplaying());
        },
        [](const RedisException &err) { testoutput(false, err.what()); },
        "echo %s",
        "hello");
    // 6
    redisClient->execCommandAsync(
        [](const drogon::nosql::RedisResult &r) {
            testoutput(true, r.getStringForDisplaying());
        },
        [](const RedisException &err) { testoutput(false, err.what()); },
        "flushall");
    // 7
    redisClient->execCommandAsync(

        [](const drogon::nosql::RedisResult &r) {
            testoutput(r.type() == RedisResultType::kNil,
                       r.getStringForDisplaying());
        },
        [](const RedisException &err) { testoutput(false, err.what()); },
        "get %s",
        "xxxxx");

    std::cout << "start\n";
#ifdef __cpp_impl_coroutine
    auto coro_test = [redisClient]() -> drogon::Task<> {
        // 8
        try
        {
            auto r = co_await redisClient->execCommandCoro("get %s", "haha");
            testoutput(r.type() == RedisResultType::kNil,
                       r.getStringForDisplaying());
        }
        catch (const RedisException &err)
        {
            testoutput(false, err.what());
        }
    };
    drogon::sync_wait(coro_test());
#endif
    globalf.get();
}

int main()
{
#ifndef USE_REDIS
    LOG_DEBUG << "Drogon is built without Redis. No tests executed.";
    return 0;
#endif
    drogon::app().setLogLevel(trantor::Logger::kWarn);
    auto redisClient = drogon::nosql::RedisClient::newRedisClient(
        trantor::InetAddress("127.0.0.1", 6379), 1);
    doTest(redisClient);
    std::cout << "Test passed\n";
    return 0;
}