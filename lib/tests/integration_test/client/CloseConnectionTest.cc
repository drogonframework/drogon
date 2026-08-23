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

// The handler sets closeConnection on the response while the client asks for
// keep-alive. The server must honour the handler and hang up on its own.
DROGON_TEST(CloseConnectionTest)
{
    auto client =
        std::make_shared<trantor::TcpClient>(app().getLoop(),
                                             trantor::InetAddress{"127.0.0.1",
                                                                  8848},
                                             "close-connection-test");
    auto response = std::make_shared<std::string>();
    auto gotResponse = std::make_shared<std::atomic<bool>>(false);
    auto completed = std::make_shared<std::atomic<bool>>(false);
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    client->setConnectionCallback([TEST_CTX, gotResponse, completed, promise](
                                      const trantor::TcpConnectionPtr &conn) {
        if (conn->connected())
        {
            conn->send(
                "GET /api/v1/close_connection HTTP/1.1\r\n"
                "Host: 127.0.0.1:8848\r\n"
                "Connection: keep-alive\r\n\r\n");
            return;
        }

        CHECK(gotResponse->load());
        if (!completed->exchange(true))
        {
            promise->set_value();
        }
    });

    client->setMessageCallback([TEST_CTX,
                                response,
                                gotResponse](const trantor::TcpConnectionPtr &,
                                             trantor::MsgBuffer *buffer) {
        response->append(buffer->read(buffer->readableBytes()));
        if (response->find("\r\n\r\n") == std::string::npos)
        {
            return;
        }
        if (!gotResponse->exchange(true))
        {
            CHECK(response->find("HTTP/1.1 200") == 0);
            CHECK(response->find("connection: close\r\n") != std::string::npos);
        }
    });

    client->connect();
    REQUIRE(future.wait_for(5s) == std::future_status::ready);
}
