#include <drogon/WebSocketClient.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/drogon_test.h>

#include <iostream>

using namespace drogon;
using namespace std::chrono_literals;

static std::vector<WebSocketClientPtr> wsClients_;
static const int kClientCount = 100;

DROGON_TEST(MultipleWsTest)
{
    wsClients_.reserve(kClientCount);
    for (size_t i = 0; i < kClientCount; i++)
    {
        auto wsPtr = WebSocketClient::newWebSocketClient("127.0.0.1", 8848);
        wsPtr->setMessageHandler(
            [TEST_CTX, i](const std::string &message,
                          const WebSocketClientPtr &wsPtr,
                          const WebSocketMessageType &type) mutable {
                CHECK((type == WebSocketMessageType::Text ||
                       type == WebSocketMessageType::Pong));
                if (type == WebSocketMessageType::Pong && TEST_CTX != nullptr)
                {
                    // Check if the correct connection got the result
                    wsPtr->stop();
                    wsClients_[i].reset();
                    CHECK(message == std::to_string(i));
                    TEST_CTX = {};
                }
            });
        wsClients_.emplace_back(std::move(wsPtr));
    }
    for (size_t i = 0; i < kClientCount; i++)
    {
        auto req = HttpRequest::newHttpRequest();
        req->setPath("/chat");
        wsClients_[i]->connectToServer(
            req,
            [TEST_CTX, i](ReqResult r,
                          const HttpResponsePtr &resp,
                          const WebSocketClientPtr &wsPtr) mutable {
                CHECK(r == ReqResult::Ok);
                if (r != ReqResult::Ok)
                {
                    wsPtr->stop();
                    wsClients_[i].reset();
                }
                REQUIRE(wsPtr != nullptr);
                REQUIRE(resp != nullptr);

                wsPtr->getConnection()->setPingMessage(std::to_string(i), 1.5s);
                wsPtr->getConnection()->send("hello!");
                CHECK(wsPtr->getConnection()->connected());

                TEST_CTX = {};
            });
    }
}
