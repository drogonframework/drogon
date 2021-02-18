#include <drogon/nosql/RedisClient.h>
#include <drogon/drogon.h>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;
using namespace drogon::nosql;
int main()
{
    drogon::app().setLogLevel(trantor::Logger::kTrace);
    auto redisClient = drogon::nosql::RedisClient::newRedisClient(
        trantor::InetAddress("127.0.0.1", 6379), 1, "123");

    std::this_thread::sleep_for(1s);
    redisClient->execCommandAsync(

        [](const drogon::nosql::RedisResult &r) {
            std::cout << "0:" << r.getStringForDisplaying() << std::endl;
        },
        [](const std::exception &err) { std::cout << err.what() << std::endl; },
        "multi");
    redisClient->execCommandAsync(

        [](const drogon::nosql::RedisResult &r) {
            std::cout << "1:" << r.getStringForDisplaying() << std::endl;
        },
        [](const std::exception &err) { std::cout << err.what() << std::endl; },
        "ping");
    redisClient->execCommandAsync(

        [](const drogon::nosql::RedisResult &r) {
            std::cout << "2:" << r.getStringForDisplaying() << std::endl;
        },
        [](const RedisException &err) { std::cout << err.what() << std::endl; },
        "echo %s",
        "hello");
    redisClient->execCommandAsync(

        [](const drogon::nosql::RedisResult &r) {
            std::cout << "3:" << r.getStringForDisplaying() << std::endl;
        },
        [](const RedisException &err) { std::cout << err.what() << std::endl; },
        "hgetall %s",
        "haha");
    redisClient->execCommandAsync(

        [](const drogon::nosql::RedisResult &r) {
            std::cout << "4:" << r.getStringForDisplaying() << std::endl;
        },
        [](const RedisException &err) { std::cout << err.what() << std::endl; },
        "get %s",
        "xxxxx");
    redisClient->execCommandAsync(

        [](const drogon::nosql::RedisResult &r) {
            std::cout << "e:\n" << r.getStringForDisplaying() << std::endl;
        },
        [](const RedisException &err) { std::cout << err.what() << std::endl; },
        "exec");
    std::cout << "start\n";
#ifdef __cpp_impl_coroutine
    auto coro_test = [redisClient]() -> drogon::Task<> {
        try
        {
            auto r = co_await redisClient->execCommandCoro("get %s", "haha");
            std::cout << "coro: " << r.getStringForDisplaying() << std::endl;
        }
        catch (const RedisException &err)
        {
            std::cout << "coro error: " << err.what() << std::endl;
        }
    };
    drogon::sync_wait(coro_test());
#endif
    getchar();
}