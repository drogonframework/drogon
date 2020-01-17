#include <drogon/drogon.h>

using namespace drogon;
int main()
{
    app()
        .setLogPath("./")
        .setLogLevel(trantor::Logger::kWarn)
        .addListener("0.0.0.0", 7770)
        .setThreadNum(0)
        .registerSyncAdvice([](const HttpRequestPtr &req) -> HttpResponsePtr {
            const auto &path = req->path();
            if (path.length() == 1 && path[0] == '/')
            {
                auto response = HttpResponse::newHttpResponse();
                response->setBody("<p>Hello, world!</p>");
                return response;
            }
            return nullptr;
        })
        .run();
}
