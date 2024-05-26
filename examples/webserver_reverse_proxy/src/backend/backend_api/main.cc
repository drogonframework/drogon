#include <drogon/drogon.h>

using namespace drogon;

int main(int argc, char *argv[])
{
    app().loadConfigFile("../config.backend_api.json");

    app().registerHandler(
        "/",
        [](const HttpRequestPtr &pReq,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            Json::Value body;

            body.clear();

            body["port"] = pReq->getLocalAddr().toPort();

            body["message"] =
                "a simple message from api.localhost.com '/' path";

            auto pResp = HttpResponse::newHttpJsonResponse(body);

            callback(pResp);
        });

    app().registerHandler(
        "/test",
        [](const HttpRequestPtr &pReq,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            Json::Value body;

            body.clear();

            body["port"] = pReq->getLocalAddr().toPort();

            body["message"] =
                "another test message from api.localhost.com '/test' path";

            auto pResp = HttpResponse::newHttpJsonResponse(body);

            callback(pResp);
        });

    app().run();
}
