#include <drogon/HttpAppFramework.h>
#include <drogon/drogon_test.h>
#include <trantor/net/TcpClient.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <string>

using namespace drogon;
using namespace std::chrono_literals;

DROGON_TEST(WebSocketUpgradeAfterKeepAliveRequestTest)
{
    auto client = std::make_shared<trantor::TcpClient>(
        app().getLoop(),
        trantor::InetAddress{"127.0.0.1", 8848},
        "websocket-keep-alive-upgrade-test");
    auto response = std::make_shared<std::string>();
    auto phase = std::make_shared<std::atomic<int>>(0);
    auto completed = std::make_shared<std::atomic<bool>>(false);
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    client->setConnectionCallback([TEST_CTX, phase, completed, promise](
                                      const trantor::TcpConnectionPtr &conn) {
        if (conn->connected())
        {
            conn->send(
                "HEAD / HTTP/1.1\r\n"
                "Host: 127.0.0.1:8848\r\n"
                "Connection: keep-alive\r\n\r\n");
            return;
        }

        if (phase->load() < 2 && !completed->exchange(true))
        {
            FAIL("Connection closed before the WebSocket upgrade completed");
            promise->set_value();
        }
    });

    client->setMessageCallback([TEST_CTX, response, phase, completed, promise](
                                   const trantor::TcpConnectionPtr &conn,
                                   trantor::MsgBuffer *buffer) {
        response->append(buffer->read(buffer->readableBytes()));
        if (response->find("\r\n\r\n") == std::string::npos)
        {
            return;
        }

        if (phase->load() == 0)
        {
            REQUIRE(response->find("HTTP/1.1 200") == 0);
            response->clear();
            phase->store(1);
            conn->send(
                "GET /chat HTTP/1.1\r\n"
                "Host: 127.0.0.1:8848\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n");
            return;
        }

        if (phase->exchange(2) == 1)
        {
            CHECK(response->find("HTTP/1.1 101") == 0);
            CHECK(response->find("upgrade: websocket") != std::string::npos);
            if (!completed->exchange(true))
            {
                promise->set_value();
            }
            conn->shutdown();
        }
    });

    client->connect();
    REQUIRE(future.wait_for(5s) == std::future_status::ready);
}
