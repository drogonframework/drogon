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

namespace
{
struct ExchangeResult
{
    std::string response;
    bool serverClosed{false};
};

// Opens a fresh connection, sends a single raw request, and once the response
// headers have arrived waits `settleMs` to observe whether the server hangs up
// on its own. A connection still open after that window is treated as kept
// alive.
std::shared_ptr<ExchangeResult> exchangeOnce(const std::string &request,
                                             double settleMs = 800.0)
{
    auto client =
        std::make_shared<trantor::TcpClient>(app().getLoop(),
                                             trantor::InetAddress{"127.0.0.1",
                                                                  8848},
                                             "close-connection-regression");
    auto result = std::make_shared<ExchangeResult>();
    auto done = std::make_shared<std::atomic<bool>>(false);
    auto timerArmed = std::make_shared<std::atomic<bool>>(false);
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();
    auto finish = [done, promise]() {
        if (!done->exchange(true))
        {
            promise->set_value();
        }
    };

    client->setConnectionCallback(
        [request, result, finish](const trantor::TcpConnectionPtr &conn) {
            if (conn->connected())
            {
                conn->send(request);
                return;
            }
            result->serverClosed = true;
            finish();
        });

    client->setMessageCallback(
        [result, timerArmed, settleMs, finish](
            const trantor::TcpConnectionPtr &conn, trantor::MsgBuffer *buffer) {
            result->response.append(buffer->read(buffer->readableBytes()));
            if (result->response.find("\r\n\r\n") == std::string::npos)
            {
                return;
            }
            if (!timerArmed->exchange(true))
            {
                conn->getLoop()->runAfter(settleMs / 1000.0, finish);
            }
        });

    client->connect();
    if (future.wait_for(10s) != std::future_status::ready)
    {
        return nullptr;
    }
    client->disconnect();
    return result;
}
}  // namespace

// An HTTP/1.0 client asking for keep-alive must be honoured. setVersion() sets
// the response's closeConnection flag as a side effect for HTTP/1.0, which must
// not be mistaken for an explicit application decision.
DROGON_TEST(CloseConnectionHttp10KeepAliveTest)
{
    auto result = exchangeOnce(
        "GET /api/v1/keepalive_probe HTTP/1.0\r\n"
        "Host: 127.0.0.1:8848\r\n"
        "Connection: keep-alive\r\n\r\n");

    REQUIRE(result != nullptr);
    CHECK(result->response.find("HTTP/1.0 200") == 0);
    CHECK(result->response.find("connection: close\r\n") == std::string::npos);
    CHECK(result->response.find("connection: Keep-Alive\r\n") !=
          std::string::npos);
    CHECK(result->serverClosed == false);
}

// The cached 404 response object is reused across requests. One client sending
// "Connection: close" must not leave that shared object permanently marked as
// close-on-send for every later client.
DROGON_TEST(CloseConnectionCachedResponseNotStickyTest)
{
    auto closing = exchangeOnce(
        "GET /this_route_does_not_exist HTTP/1.1\r\n"
        "Host: 127.0.0.1:8848\r\n"
        "Connection: close\r\n\r\n");
    REQUIRE(closing != nullptr);
    CHECK(closing->serverClosed == true);

    auto keepAlive = exchangeOnce(
        "GET /this_route_does_not_exist HTTP/1.1\r\n"
        "Host: 127.0.0.1:8848\r\n"
        "Connection: keep-alive\r\n\r\n");
    REQUIRE(keepAlive != nullptr);
    CHECK(keepAlive->response.find("connection: close\r\n") ==
          std::string::npos);
    CHECK(keepAlive->serverClosed == false);
}

// A failed WebSocket route sets closeConnection on the response returned by
// newNotFoundResponse(), which may be a shared or per-IO-thread cached object.
// Later plain 404s must still honour keep-alive.
DROGON_TEST(CloseConnectionWebSocketNotFoundNotStickyTest)
{
    auto wsNotFound = exchangeOnce(
        "GET /this_ws_route_does_not_exist HTTP/1.1\r\n"
        "Host: 127.0.0.1:8848\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n");
    REQUIRE(wsNotFound != nullptr);

    auto keepAlive = exchangeOnce(
        "GET /this_route_does_not_exist HTTP/1.1\r\n"
        "Host: 127.0.0.1:8848\r\n"
        "Connection: keep-alive\r\n\r\n");
    REQUIRE(keepAlive != nullptr);
    CHECK(keepAlive->response.find("connection: close\r\n") ==
          std::string::npos);
    CHECK(keepAlive->serverClosed == false);
}
