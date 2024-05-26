#include <drogon/drogon.h>

using namespace drogon;

int main(int argc, char *argv[])
{
    app().loadConfigFile("../config.drogon_server.json");

    app().run();
}
