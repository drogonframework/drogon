#include <drogon/nosql/RedisClient.h>
#include <drogon/drogon.h>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;
int main()
{
    drogon::app().setLogLevel(trantor::Logger::kTrace);
    auto redisClient = drogon::nosql::RedisClient::newRedisClient(
        trantor::InetAddress("127.0.0.1", 6379), 1, "123");

    std::this_thread::sleep_for(1s);
    redisClient->execCommandAsync(
        "ping",
        [](const drogon::nosql::RedisResult &r) {
            std::cout << "1:" << r.asString() << std::endl;
        },
        [](const std::exception &err) {
            std::cout << err.what() << std::endl;
        });
    redisClient->execCommandAsync(
        "echo %s",
        [](const drogon::nosql::RedisResult &r) {
            std::cout << "2:" << r.asString() << std::endl;
        },
        [](const std::exception &err) { std::cout << err.what() << std::endl; },
        "hello");
    redisClient->execCommandAsync(
        "ping",
        [](const drogon::nosql::RedisResult &r) {
            std::cout << "3:" << r.asString() << std::endl;
        },
        [](const std::exception &err) {
            std::cout << err.what() << std::endl;
        });
    std::cout << "start\n";
    getchar();
}