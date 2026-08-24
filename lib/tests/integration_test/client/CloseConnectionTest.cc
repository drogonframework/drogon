#include <drogon/HttpAppFramework.h>
#include <drogon/drogon_test.h>
#include <trantor/net/TcpClient.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <vector>

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
    std::string data;
    int responses{0};
    bool serverClosed{false};
};

// Counts response status lines seen so far. The bodies used by these tests
// never contain the token, so this is an adequate framing signal for a test.
int countResponses(const std::string &data)
{
    int count = 0;
    size_t pos = 0;
    while ((pos = data.find("HTTP/1.", pos)) != std::string::npos)
    {
        ++count;
        pos += 7;
    }
    return count;
}

// Opens one connection and sends `requests` in order, each only after the
// previous response has arrived. Completes as soon as every response has been
// received -- which positively proves the connection survived -- or as soon as
// the server hangs up, whichever comes first.
//
// Liveness is proven by obtaining another response rather than by waiting out a
// timer, so the outcome does not depend on any delay.
std::shared_ptr<ExchangeResult> exchange(
    const std::vector<std::string> &requests)
{
    auto client =
        std::make_shared<trantor::TcpClient>(app().getLoop(),
                                             trantor::InetAddress{"127.0.0.1",
                                                                  8848},
                                             "close-connection-regression");
    auto result = std::make_shared<ExchangeResult>();
    auto done = std::make_shared<std::atomic<bool>>(false);
    auto sent = std::make_shared<std::atomic<size_t>>(0);
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();
    auto finish = [done, promise]() {
        if (!done->exchange(true))
        {
            promise->set_value();
        }
    };

    client->setConnectionCallback([requests, sent, result, finish](
                                      const trantor::TcpConnectionPtr &conn) {
        if (conn->connected())
        {
            conn->send(requests[sent->fetch_add(1)]);
            return;
        }
        result->serverClosed = true;
        finish();
    });

    client->setMessageCallback(
        [requests, sent, result, finish](const trantor::TcpConnectionPtr &conn,
                                         trantor::MsgBuffer *buffer) {
            result->data.append(buffer->read(buffer->readableBytes()));
            // Wait for the newest response's headers to be complete before
            // acting.
            if (result->data.rfind("\r\n\r\n") == std::string::npos)
            {
                return;
            }
            const int seen = countResponses(result->data);
            if (seen < static_cast<int>(sent->load()))
            {
                return;
            }
            result->responses = seen;
            if (sent->load() < requests.size())
            {
                conn->send(requests[sent->fetch_add(1)]);
                return;
            }
            finish();
        });

    client->connect();
    if (future.wait_for(15s) != std::future_status::ready)
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
    const std::string request =
        "GET /api/v1/keepalive_probe HTTP/1.0\r\n"
        "Host: 127.0.0.1:8848\r\n"
        "Connection: keep-alive\r\n\r\n";

    auto result = exchange({request, request});

    REQUIRE(result != nullptr);
    CHECK(result->data.find("HTTP/1.0 200") == 0);
    CHECK(result->data.find("connection: close\r\n") == std::string::npos);
    CHECK(result->data.find("connection: Keep-Alive\r\n") != std::string::npos);
    // Two responses on one connection prove the server honoured keep-alive.
    CHECK(result->responses == 2);
    CHECK(result->serverClosed == false);
}

// The cached 404 response object is reused across requests. One client sending
// "Connection: close" must not leave that shared object permanently marked as
// close-on-send for every later client.
DROGON_TEST(CloseConnectionCachedResponseNotStickyTest)
{
    // First client opts out of keep-alive. A second request is attempted on the
    // same connection: the server is expected to have hung up instead of
    // answering it.
    const std::string closeRequest =
        "GET /this_route_does_not_exist HTTP/1.1\r\n"
        "Host: 127.0.0.1:8848\r\n"
        "Connection: close\r\n\r\n";

    auto closing = exchange({closeRequest, closeRequest});
    REQUIRE(closing != nullptr);
    CHECK(closing->serverClosed == true);
    CHECK(closing->responses == 1);

    // A later client must still get keep-alive, on a connection of its own.
    const std::string keepAliveRequest =
        "GET /this_route_does_not_exist HTTP/1.1\r\n"
        "Host: 127.0.0.1:8848\r\n"
        "Connection: keep-alive\r\n\r\n";

    auto keepAlive = exchange({keepAliveRequest, keepAliveRequest});
    REQUIRE(keepAlive != nullptr);
    CHECK(keepAlive->data.find("connection: close\r\n") == std::string::npos);
    CHECK(keepAlive->responses == 2);
    CHECK(keepAlive->serverClosed == false);
}

// A failed WebSocket route sets closeConnection on the response returned by
// newNotFoundResponse(), which may be a shared or per-IO-thread cached object.
// Later plain 404s must still honour keep-alive.
DROGON_TEST(CloseConnectionWebSocketNotFoundNotStickyTest)
{
    auto wsNotFound = exchange({
        "GET /this_ws_route_does_not_exist HTTP/1.1\r\n"
        "Host: 127.0.0.1:8848\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n",
    });
    REQUIRE(wsNotFound != nullptr);

    const std::string keepAliveRequest =
        "GET /this_route_does_not_exist HTTP/1.1\r\n"
        "Host: 127.0.0.1:8848\r\n"
        "Connection: keep-alive\r\n\r\n";

    auto keepAlive = exchange({keepAliveRequest, keepAliveRequest});
    REQUIRE(keepAlive != nullptr);
    CHECK(keepAlive->data.find("connection: close\r\n") == std::string::npos);
    CHECK(keepAlive->responses == 2);
    CHECK(keepAlive->serverClosed == false);
}
