#include <drogon/drogon.h>

using namespace drogon;

int main(int argc, char *argv[])
{
    app().loadConfigFile("../config.backend_www.json");

    app().registerHandler(
        "/",
        [](const HttpRequestPtr &pReq,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            auto pResp = HttpResponse::newHttpResponse();

            std::string body =
                std::string("welcome to localhost.com port " +
                            std::to_string(pReq->getLocalAddr().toPort()));

            pResp->setBody(body);

            callback(pResp);
        });

    app().run();
}
