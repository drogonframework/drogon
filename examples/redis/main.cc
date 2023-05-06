#include <drogon/drogon.h>

int main()
{
    // Set HTTP listener address and port
    drogon::app().addListener("0.0.0.0", 8080);
    // Run HTTP framework,the method will block in the internal event loop
    drogon::app().createRedisClient("127.0.0.1", 6379);
    drogon::app().run();
    return 0;
}
