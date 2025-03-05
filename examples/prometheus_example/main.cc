#include <drogon/drogon.h>

int main()
{
    drogon::app().loadConfigFile("../config.json").run();
    return 0;
}
