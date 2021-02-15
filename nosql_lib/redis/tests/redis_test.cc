#include <drogon/nosql/RedisClient.h>
#include <iostream>

int main()
{
    auto redisClient = drogon::nosql::RedisClient::newRedisClient(
        trantor::InetAddress("127.0.0.1", 6379));

    getchar();
    redisClient->execCommandAsync(
        "ping",
        [](const drogon::nosql::RedisResult &r) {
            std::cout<<r.asString()<<std::endl;
        },
        [](const std::exception &err) {
            std::cerr<<err.what()<<std::endl;
        });
    getchar();
}