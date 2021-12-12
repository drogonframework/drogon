#include <drogon/WebSocketClient.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/drogon_test.h>

#include <iostream>

using namespace drogon;
using namespace std::chrono_literals;

struct DataPack
{
    WebSocketClientPtr wsPtr;
    std::shared_ptr<drogon::test::CaseBase> TEST_CTX;
};

static WebSocketClientPtr wsPtr_;
DROGON_TEST(WebSocketTest)
{
    wsPtr_ = WebSocketClient::newWebSocketClient("127.0.0.1", 8848);
    auto pack = std::make_shared<DataPack>({wsPtr_, TEST_CTX});
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/chat");
    wsPtr_->setMessageHandler(
        [pack](const std::string &message,
                   const WebSocketClientPtr &wsPtr,
                   const WebSocketMessageType &type) mutable {
            CHECK(type == WebSocketMessageType::Text ||
                   type == WebSocketMessageType::Pong);
            if (type == WebSocketMessageType::Pong && pack != nullptr)
            {
                auto TEST_CTX = pack->TEST_CTX;
                auto wsPtr = pack->wsPtr;

                wsPtr_->stop();
                CHECK(message.empty());
                pack.reset();
            }
        });

    wsPtr_->connectToServer(
        req,
        [TEST_CTX, pack](ReqResult r,
                   const HttpResponsePtr &resp,
                   const WebSocketClientPtr &wsPtr) mutable {
            if (r != ReqResult::Ok)
            {
                wsPtr_->stop();
                wsPtr_.reset();
                pack.reset();
            }
            REQUIRE(r == ReqResult::Ok);
            REQUIRE(wsPtr != nullptr);
            REQUIRE(resp != nullptr);
            wsPtr->getConnection()->setPingMessage("", 1s);
            wsPtr->getConnection()->send("hello!");
            CHECK(wsPtr->getConnection()->connected());
            // Drop the testing context as WS controllers stores the lambda and
            // never release it. Causing a dead lock later.
            TEST_CTX = {};
        });
}
