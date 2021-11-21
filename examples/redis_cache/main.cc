#include <drogon/drogon.h>
int main()
{
    drogon::app().loadConfigFile("../config.json");
    drogon::app().run();
    return 0;
}
