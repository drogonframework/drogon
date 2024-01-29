#include <drogon/drogon.h>

int main()
{
    drogon::app().addListener("0.0.0.0", 8080);
    drogon::app().createRedisClient("127.0.0.1", 6379);
    drogon::app().run();
    return 0;
}
