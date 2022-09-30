#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/WebSocketClient.h>
#include <drogon/WebSocketController.h>
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/plugins/RealIpResolver.h>
#include <drogon/drogon.h>
#include <drogon/HttpTypes.h>

using namespace drogon;
using namespace std::chrono_literals;

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

struct DataPack
{
    WebSocketClientPtr wsPtr;
    std::shared_ptr<drogon::test::CaseBase> TEST_CTX;
};

static WebSocketClientPtr wsPtr_;
DROGON_TEST(WebSocketRealIp)
{
    wsPtr_ = WebSocketClient::newWebSocketClient("127.0.0.1", 8017);
    auto pack = std::make_shared<DataPack *>(new DataPack{wsPtr_, TEST_CTX});
    auto req = HttpRequest::newHttpRequest();
    req->addHeader("x-forwarded-for", "2.2.2.2,1.1.1.1:7001, wrong,9.9.9.9");
    req->setPath("/ws/test-real-ip");
    wsPtr_->setMessageHandler([pack](const std::string &message,
                                     const WebSocketClientPtr &wsPtr,
                                     const WebSocketMessageType &type) mutable {
        if (pack == nullptr)
            return;
        auto TEST_CTX = (*pack)->TEST_CTX;
        if (type == WebSocketMessageType::Pong)
        {
            auto wsPtr = (*pack)->wsPtr;

            wsPtr_->stop();
            CHECK(message.empty());
            delete *pack;
            pack = nullptr;
        }
        else
        {
            CHECK(type == WebSocketMessageType::Text);
            CHECK(message == "1.1.1.1");
        }
    });

    wsPtr_->connectToServer(req,
                            [pack](ReqResult r,
                                   const HttpResponsePtr &resp,
                                   const WebSocketClientPtr &wsPtr) mutable {
                                auto TEST_CTX = (*pack)->TEST_CTX;
                                CHECK((*pack)->wsPtr == wsPtr);
                                if (r != ReqResult::Ok)
                                {
                                    wsPtr_->stop();
                                    wsPtr_.reset();
                                    delete *pack;
                                    pack = nullptr;
                                }
                                REQUIRE(r == ReqResult::Ok);
                                REQUIRE(wsPtr != nullptr);
                                REQUIRE(resp != nullptr);
                                wsPtr->getConnection()->setPingMessage("", 1s);
                                wsPtr->getConnection()->send("hello!");
                                CHECK(wsPtr->getConnection()->connected());
                                // Drop the testing context as WS controllers
                                // stores the lambda and never release it.
                                // Causing a dead lock later.
                                TEST_CTX = {};
                                pack.reset();
                            });
}

class RealIpController : public drogon::HttpController<RealIpController>
{
  public:
    METHOD_LIST_BEGIN
    METHOD_ADD(RealIpController::getRealIp, "/my-ip", Get);
    METHOD_LIST_END

    void getRealIp(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto &addr = plugin::RealIpResolver::GetRealAddr(req);
        auto resp = HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
        resp->setBody(addr.toIp());
        callback(resp);
    }
};

class RealIpWsController
    : public drogon::WebSocketController<RealIpWsController>
{
  public:
    void handleNewMessage(const WebSocketConnectionPtr &wsConn,
                          std::string &&string,
                          const WebSocketMessageType &type) override
    {
    }
    void handleNewConnection(const HttpRequestPtr &req,
                             const WebSocketConnectionPtr &wsConn) override
    {
        trantor::InetAddress addr = plugin::RealIpResolver::GetRealAddr(req);
        wsConn->send(addr.toIp());
        LOG_INFO << "req->attribute[real-ip]: "
                 << req->attributes()->find("real-ip");
    }
    void handleConnectionClosed(const WebSocketConnectionPtr &wsConn) override
    {
    }

    WS_PATH_LIST_BEGIN
    // list path definations here;
    WS_PATH_ADD("/ws/test-real-ip", Get);
    WS_PATH_LIST_END
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
