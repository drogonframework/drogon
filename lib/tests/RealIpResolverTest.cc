#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/plugins/RealIpResolver.h>
#include <drogon/drogon.h>
#include <drogon/HttpTypes.h>

using namespace drogon;

DROGON_TEST(RealIpResolver)
{
    auto client =
        HttpClient::newHttpClient("http://127.0.0.1:8017",
                                  HttpAppFramework::instance().getLoop());

    auto newRequest = []() {
        auto req = HttpRequest::newHttpRequest();
        req->setPath("/RealIpController/my-ip");
        return req;
    };

    // 1. No headers
    {
        auto req = newRequest();
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "127.0.0.1");
            });
    }
    // 2. header only contains real ip
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "1.1.1.1");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "1.1.1.1");
            });
    }
    // 3. Ip with port
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "1.1.1.1:7777");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "1.1.1.1");
            });
    }
    // 3. multiple ips
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for",
                       "2.2.2.2,1.1.1.1:7001,  172.16.0.100:7002,127.0.0.1");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "1.1.1.1");
            });
    }
    // 4. Ignore error in header
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for",
                       "2.2.2.2,1.1.1.1:7001, wrong,9.9.9.9");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "1.1.1.1");
            });
    }
};

class RealIpController : public drogon::HttpController<RealIpController>
{
  public:
    METHOD_LIST_BEGIN
    METHOD_ADD(RealIpController::getRealIp, "/my-ip", Get);
    METHOD_LIST_END

    void getRealIp(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto addr = req->attributes()->get<trantor::InetAddress>("real-ip");
        auto resp = HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
        resp->setBody(addr.toIp());
        callback(resp);
    }
};

// -- main
int main(int argc, char **argv)
{
    trantor::Logger::setLogLevel(trantor::Logger::kInfo);
    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();

    std::stringstream ss;
    ss << R"({
    "listeners": [
        {
            "address": "0.0.0.0",
            "port": 8017
        }
    ],
    "plugins": [
        {
            "name": "drogon::plugin::RealIpResolver",
            "config": {
                "trust_ips": ["127.0.0.1", "172.16.0.0/12", "9.9.9.9/32"],
                "from_header": "x-forwarded-for",
                "attribute_key": "real-ip"
            }
        }
    ]
})";
    Json::Value config;
    ss >> config;

    std::thread thr([&]() {
        app().loadConfigJson(config);
        app().getLoop()->queueInLoop([&p1]() { p1.set_value(); });
        app().run();
    });

    f1.get();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int testStatus = test::run(argc, argv);
    app().getLoop()->queueInLoop([]() { app().quit(); });
    thr.join();
    return testStatus;
}
