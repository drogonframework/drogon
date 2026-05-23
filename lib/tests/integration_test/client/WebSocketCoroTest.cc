#include <drogon/WebSocketClient.h>
#include <drogon/drogon_test.h>

#include <chrono>
#include <cstdio>
#include <future>

using namespace drogon;
using namespace std::chrono_literals;

DROGON_TEST(WebSocketCoroControllerTest)
{
#if defined(__cpp_impl_coroutine)
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    auto first = WebSocketClient::newWebSocketClient("127.0.0.1", 8848);
    auto second = WebSocketClient::newWebSocketClient("127.0.0.1", 8848);

    first->setMessageHandler(
        [first, second, promise, TEST_CTX](const std::string &message,
                                           const WebSocketClientPtr &,
                                           const WebSocketMessageType &type) {
            CHECK(type == WebSocketMessageType::Text);
            if (message == "opened")
            {
                first->getConnection()->send("hello-coro");
            }
            else if (message == "coro:hello-coro")
            {
                first->getConnection()->shutdown();
            }
        });

    first->setConnectionClosedHandler([second, promise, TEST_CTX](
                                          const WebSocketClientPtr &) {
        auto req2 = HttpRequest::newHttpRequest();
        req2->setPath("/coro-chat");

        second->setMessageHandler(
            [second, promise, TEST_CTX](const std::string &message,
                                        const WebSocketClientPtr &,
                                        const WebSocketMessageType &type) {
                CHECK(type == WebSocketMessageType::Text);
                if (message == "opened")
                {
                    second->getConnection()->send("stats");
                    return;
                }

                if (message.rfind("stats:", 0) == 0)
                {
                    int opened = 0;
                    int msg = 0;
                    int closed = 0;
                    auto parsed = sscanf(message.c_str(),
                                         "stats:%d:%d:%d",
                                         &opened,
                                         &msg,
                                         &closed);
                    REQUIRE(parsed == 3);
                    CHECK(opened >= 2);
                    CHECK(msg >= 2);
                    CHECK(closed >= 1);
                    second->stop();
                    promise->set_value();
                }
            });

        second->connectToServer(req2,
                                [second, TEST_CTX](ReqResult r,
                                                   const HttpResponsePtr &resp,
                                                   const WebSocketClientPtr &) {
                                    REQUIRE(r == ReqResult::Ok);
                                    REQUIRE(resp != nullptr);
                                    CHECK(second->getConnection()->connected());
                                });
    });

    auto req = HttpRequest::newHttpRequest();
    req->setPath("/coro-chat");
    first->connectToServer(req,
                           [first, TEST_CTX](ReqResult r,
                                             const HttpResponsePtr &resp,
                                             const WebSocketClientPtr &) {
                               REQUIRE(r == ReqResult::Ok);
                               REQUIRE(resp != nullptr);
                               CHECK(first->getConnection()->connected());
                           });

    REQUIRE(future.wait_for(5s) == std::future_status::ready);
#endif
}
