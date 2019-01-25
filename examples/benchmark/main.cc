#include <drogon/drogon.h>

using namespace drogon;
int main()
{
    app().setLogPath("./");
    app().setLogLevel(trantor::Logger::WARN);
    app().addListener("0.0.0.0", 7770);
    app().setThreadNum(16);
    app().enableRunAsDaemon();
    // app().enableRelaunchOnError();
    // app().loop()->runEvery(1, []() {
    //     LOG_WARN << "HAHA";
    // });
    app().run();
}