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

static const int kClientCount = 100;

DROGON_TEST(MultipleWsTest)
{
    for (size_t i = 0; i < kClientCount; i++)
    {
        auto wsPtr = WebSocketClient::newWebSocketClient("127.0.0.1", 8848);
        auto pack = std::make_shared<DataPack *>(new DataPack{wsPtr, TEST_CTX});

        wsPtr->setMessageHandler(
            [pack, i](const std::string &message,
                      const WebSocketClientPtr &wsPtr,
                      const WebSocketMessageType &type) mutable {
                if (pack == nullptr)
                    return;
                auto TEST_CTX = (*pack)->TEST_CTX;
                CHECK((type == WebSocketMessageType::Text ||
                       type == WebSocketMessageType::Pong));
                if (type == WebSocketMessageType::Pong && TEST_CTX != nullptr)
                {
                    auto wsPtr = (*pack)->wsPtr;

                    // Check if the correct connection got the result
                    wsPtr->stop();
                    CHECK(message == std::to_string(i));
                    delete *pack;
                    pack = nullptr;
                }
            });
        auto req = HttpRequest::newHttpRequest();
        req->setPath("/chat");
        wsPtr->connectToServer(
            req,
            [pack, i](ReqResult r,
                      const HttpResponsePtr &resp,
                      const WebSocketClientPtr &wsPtr) mutable {
                auto TEST_CTX = (*pack)->TEST_CTX;
                CHECK((*pack)->wsPtr == wsPtr);
                CHECK(r == ReqResult::Ok);
                if (r != ReqResult::Ok)
                {
                    wsPtr->stop();
                    delete *pack;
                    pack = nullptr;
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
