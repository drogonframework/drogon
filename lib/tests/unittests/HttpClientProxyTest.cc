#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/drogon_test.h>
#include <drogon/utils/Utilities.h>
#include <trantor/net/TcpClient.h>
#include <trantor/net/TcpServer.h>
#include <array>
#include <atomic>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace drogon;
using namespace trantor;

namespace
{
class LocalServer
{
  public:
    explicit LocalServer(RecvMessageCallback callback,
                         TLSPolicyPtr tls = nullptr)
    {
        std::promise<uint16_t> ready;
        auto loop = app().getLoop();
        loop->queueInLoop([this,
                           loop,
                           &ready,
                           callback = std::move(callback),
                           tls = std::move(tls)]() mutable {
            server_ = std::make_unique<TcpServer>(loop,
                                                  InetAddress("127.0.0.1", 0),
                                                  "http-client-proxy-test");
            server_->setRecvMessageCallback(std::move(callback));
            if (tls)
                server_->enableSSL(std::move(tls));
            server_->start();
            ready.set_value(server_->address().toPort());
        });
        port_ = ready.get_future().get();
    }

    ~LocalServer()
    {
        std::promise<void> stopped;
        app().getLoop()->queueInLoop([this, &stopped]() {
            server_->stop();
            server_.reset();
            stopped.set_value();
        });
        stopped.get_future().wait();
    }

    uint16_t port() const
    {
        return port_;
    }

    std::string url() const
    {
        return "http://127.0.0.1:" + std::to_string(port_);
    }

  private:
    std::unique_ptr<TcpServer> server_;
    uint16_t port_{0};
};

std::string takeHeaders(MsgBuffer *buffer)
{
    const std::string_view data(buffer->peek(), buffer->readableBytes());
    const auto end = data.find("\r\n\r\n");
    if (end == std::string_view::npos)
        return {};
    std::string headers(data.substr(0, end + 4));
    buffer->retrieve(headers.size());
    return headers;
}

void echoRequest(const TcpConnectionPtr &connection, MsgBuffer *buffer)
{
    const auto headers = takeHeaders(buffer);
    if (!headers.empty())
    {
        connection->send(
            "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " +
            std::to_string(headers.size()) + "\r\n\r\n" + headers);
        connection->shutdown();
    }
}

auto send(const HttpClientPtr &client,
          double timeout = 5.0,
          HttpRequestPtr request = HttpRequest::newHttpRequest())
{
    using Result = std::pair<ReqResult, HttpResponsePtr>;
    auto completed = std::make_shared<std::promise<Result>>();
    auto result = completed->get_future();
    request->setPath("/hello");
    request->setParameter("name", "drogon");
    client->sendRequest(
        request,
        [completed](ReqResult status, const HttpResponsePtr &response) {
            completed->set_value({status, response});
        },
        timeout);
    return result;
}

void relay(const TcpConnectionPtr &connection, MsgBuffer *buffer)
{
    connection->send(*buffer);
    buffer->retrieveAll();
}
}  // namespace

DROGON_TEST(HttpClientProxyUsesAbsoluteRequestTarget)
{
    LocalServer origin(echoRequest);
    std::atomic<size_t> proxied{0};
    LocalServer proxy(
        [&proxied](const TcpConnectionPtr &connection, MsgBuffer *buffer) {
            ++proxied;
            echoRequest(connection, buffer);
        });

    auto [directResult, directResponse] =
        send(HttpClient::newHttpClient(origin.url())).get();
    REQUIRE(directResult == ReqResult::Ok);
    CHECK(directResponse->body().find("GET /hello?name=drogon HTTP/1.1\r\n") ==
          0);
    CHECK(proxied.load() == 0);

    auto client =
        HttpClient::newHttpClientWithProxy("http://origin.invalid:8080",
                                           proxy.url());
    client->addHeader("proxy-authorization", "example");
    auto request = HttpRequest::newHttpRequest();
    request->addHeader("host", "override.invalid");
    auto [result, response] = send(client, 5.0, request).get();
    REQUIRE(result == ReqResult::Ok);
    CHECK(
        response->body().find(
            "GET http://origin.invalid:8080/hello?name=drogon HTTP/1.1\r\n") ==
        0);
    CHECK(response->body().find("\r\nhost: origin.invalid:8080\r\n") !=
          std::string_view::npos);
    CHECK(request->getHeader("host") == "override.invalid");
    CHECK(response->body().find("\r\nproxy-authorization: example\r\n") !=
          std::string_view::npos);
    CHECK(request->headers().count("proxy-authorization") == 0);
    const auto [reusedResult, reusedResponse] =
        send(HttpClient::newHttpClient(origin.url()), 5.0, request).get();
    REQUIRE(reusedResult == ReqResult::Ok);
    CHECK(reusedResponse->body().find("\r\nproxy-authorization:") ==
          std::string_view::npos);
    CHECK(proxied.load() > 0);
    CHECK(client->host() == "origin.invalid");
    CHECK(client->port() == 8080);
}

DROGON_TEST(HttpClientProxyDoesNotStoreAuthenticationCookies)
{
    size_t requests = 0;
    LocalServer proxy(
        [&requests](const TcpConnectionPtr &connection, MsgBuffer *buffer) {
            if (requests == 2)
            {
                echoRequest(connection, buffer);
                return;
            }
            if (takeHeaders(buffer).empty())
                return;
            if (++requests == 1)
            {
                connection->send(
                    "HTTP/1.1 407 Proxy Authentication Required\r\n"
                    "Proxy-Authenticate: Basic realm=\"test\"\r\n"
                    "Set-Cookie: proxy_session=proxy; Path=/\r\n"
                    "Content-Length: 0\r\n\r\n");
            }
            else
            {
                connection->send(
                    "HTTP/1.1 200 OK\r\n"
                    "Set-Cookie: origin_session=origin; Path=/\r\n"
                    "Content-Length: 0\r\n\r\n");
            }
        });
    auto client = HttpClient::newHttpClientWithProxy("http://origin.invalid",
                                                     proxy.url());
    client->enableCookies();

    const auto [challengeResult, challenge] = send(client).get();
    REQUIRE(challengeResult == ReqResult::Ok);
    CHECK(challenge->statusCode() == k407ProxyAuthenticationRequired);
    CHECK(challenge->cookies().count("proxy_session") == 1);
    REQUIRE(send(client).get().first == ReqResult::Ok);

    const auto [result, response] = send(client).get();
    REQUIRE(result == ReqResult::Ok);
    CHECK(response->body().find("proxy_session=") == std::string_view::npos);
    CHECK(response->body().find("origin_session=origin") !=
          std::string_view::npos);
}

DROGON_TEST(HttpClientProxyReusesHttp10KeepAlive)
{
    size_t connections = 0;
    LocalServer proxy(
        [&connections](const TcpConnectionPtr &connection, MsgBuffer *buffer) {
            if (takeHeaders(buffer).empty())
                return;
            auto number = connection->getContext<size_t>();
            if (!number)
            {
                number = std::make_shared<size_t>(++connections);
                connection->setContext(number);
            }
            const auto body = std::to_string(*number);
            connection->send(
                "HTTP/1.0 200 OK\r\nConnection: Keep-Alive\r\n"
                "Content-Length: " +
                std::to_string(body.size()) + "\r\n\r\n" + body);
        });
    auto client = HttpClient::newHttpClientWithProxy("http://origin.invalid",
                                                     proxy.url());
    for (size_t i = 0; i < 2; ++i)
    {
        const auto [result, response] = send(client).get();
        REQUIRE(result == ReqResult::Ok);
        CHECK(response->body() == "1");
    }
}

DROGON_TEST(HttpClientProxyEstablishesTlsTunnel)
{
    if (!drogon::utils::supportsTls())
        return;

    auto tls =
        TLSPolicy::defaultServerPolicy(DROGON_TEST_CERT_DIR "/proxy-test.crt",
                                       DROGON_TEST_CERT_DIR "/proxy-test.key");
    LocalServer origin(echoRequest, std::move(tls));
    const auto destination = "localhost:" + std::to_string(origin.port());
    std::atomic<size_t> tunnels{0};
    LocalServer proxy([TEST_CTX, &origin, &destination, &tunnels](
                          const TcpConnectionPtr &connection,
                          MsgBuffer *buffer) {
        if (auto client = connection->getContext<TcpClient>())
        {
            if (auto upstream = client->connection())
                relay(upstream, buffer);
            return;
        }

        const auto headers = takeHeaders(buffer);
        if (headers.empty())
            return;
        if (headers.find("CONNECT localhost:443 ") == 0)
        {
            CHECK(headers ==
                  "CONNECT localhost:443 HTTP/1.1\r\n"
                  "Host: localhost\r\n\r\n");
        }
        else
        {
            CHECK(headers == "CONNECT " + destination + " HTTP/1.1\r\nHost: " +
                                 destination + "\r\n\r\n");
        }
        CHECK(buffer->readableBytes() == 0);
        ++tunnels;

        auto client =
            std::make_shared<TcpClient>(connection->getLoop(),
                                        InetAddress("127.0.0.1", origin.port()),
                                        "proxy-test-origin");
        connection->setContext(client);
        std::weak_ptr<TcpConnection> downstream = connection;
        client->setConnectionCallback(
            [downstream](const TcpConnectionPtr &conn) {
                if (auto peer = downstream.lock())
                {
                    if (conn->connected())
                    {
                        // Only the final response establishes the tunnel.
                        // Its framing fields must be ignored.
                        peer->send(
                            "HTTP/1.1 100 Continue\r\n\r\n"
                            "HTTP/1.1 200 Connection established\r\n"
                            "Content-Length: 12\r\n"
                            "content-Length: 24\r\n"
                            "Transfer-Encoding: chunked\r\n\r\n");
                    }
                    else
                    {
                        peer->shutdown();
                    }
                }
            });
        client->setMessageCallback(
            [downstream](const TcpConnectionPtr &, MsgBuffer *msg) {
                if (auto peer = downstream.lock())
                    relay(peer, msg);
            });
        client->connect();
    });

    auto direct = HttpClient::newHttpClient(
        "127.0.0.1", origin.port(), true, nullptr, false, false);
    REQUIRE(send(direct).get().first == ReqResult::Ok);
    CHECK(tunnels.load() == 0);

    auto client = HttpClient::newHttpClientWithProxy(
        "https://" + destination, proxy.url(), nullptr, false, false);
    client->addHeader("proxy-authorization", "example");
    const std::array requests{HttpRequest::newHttpRequest(),
                              HttpRequest::newHttpRequest()};
    std::promise<std::array<decltype(send(client)), 2>> submitted;
    client->getLoop()->queueInLoop([&]() {
        submitted.set_value(
            {send(client, 5.0, requests[0]), send(client, 5.0, requests[1])});
    });
    auto responses = submitted.get_future().get();
    // Both requests are submitted before the first response closes its tunnel.
    for (size_t i = 0; i < responses.size(); ++i)
    {
        const auto [result, response] = responses[i].get();
        REQUIRE(result == ReqResult::Ok);
        CHECK(response->body().find("GET /hello?name=drogon HTTP/1.1\r\n") ==
              0);
        CHECK(response->body().find("proxy-authorization:") ==
              std::string_view::npos);
        CHECK(requests[i]->headers().count("proxy-authorization") == 0);
    }
    CHECK(tunnels.load() == requests.size());

    // The proxy forwards to the loopback listener even when the destination
    // uses the default HTTPS port.
    auto defaultPort = HttpClient::newHttpClientWithProxy(
        "https://localhost", proxy.url(), nullptr, false, false);
    const auto [defaultResult, defaultResponse] = send(defaultPort).get();
    REQUIRE(defaultResult == ReqResult::Ok);
    CHECK(defaultResponse->body().find("\r\nhost: localhost\r\n") !=
          std::string_view::npos);
    CHECK(tunnels.load() == requests.size() + 1);

    auto validating =
        HttpClient::newHttpClientWithProxy("https://" + destination,
                                           proxy.url());
    const auto [result, response] = send(validating).get();
    CHECK(result == ReqResult::InvalidCertificate);
    CHECK(response == nullptr);
}

DROGON_TEST(HttpClientProxyTimesOutDuringTunnelSetup)
{
    if (!drogon::utils::supportsTls())
        return;

    LocalServer proxy([](const TcpConnectionPtr &, MsgBuffer *buffer) {
        buffer->retrieveAll();
    });
    auto client = HttpClient::newHttpClientWithProxy("https://origin.invalid",
                                                     proxy.url());
    CHECK(send(client, 0.1).get().first == ReqResult::Timeout);
    CHECK(client->outstandingRequests() == 0);
    CHECK(client->requestsBufferSize() == 0);
}

DROGON_TEST(HttpClientProxyRecoversFromTunnelFailure)
{
    std::atomic<size_t> attempts{0};
    LocalServer proxy(
        [&attempts](const TcpConnectionPtr &connection, MsgBuffer *buffer) {
            if (!takeHeaders(buffer).empty())
            {
                ++attempts;
                connection->send("HTTP/1.1 403 Forbidden\r\n\r\n");
            }
        });
    auto client = HttpClient::newHttpClientWithProxy("https://origin.invalid",
                                                     proxy.url());

    if (!drogon::utils::supportsTls())
    {
        CHECK(send(client).get().first == ReqResult::HandshakeError);
        CHECK(attempts.load() == 0);
        return;
    }

    std::promise<ReqResult> retried;
    client->sendRequest(
        HttpRequest::newHttpRequest(),
        [TEST_CTX, client, &retried](ReqResult result,
                                     const HttpResponsePtr &) {
            CHECK(result == ReqResult::BadResponse);
            client->sendRequest(
                HttpRequest::newHttpRequest(),
                [&retried](ReqResult retry, const HttpResponsePtr &) {
                    retried.set_value(retry);
                },
                5.0);
        },
        5.0);
    CHECK(retried.get_future().get() == ReqResult::BadResponse);
    CHECK(attempts.load() == 2);
    CHECK(client->outstandingRequests() == 0);
    CHECK(client->requestsBufferSize() == 0);
}

DROGON_TEST(HttpClientProxyRejectsInvalidEndpoints)
{
    for (const auto *url : {"https://proxy.invalid",
                            "http://",
                            "http://user@proxy.invalid",
                            "http://proxy.invalid:65536",
                            "http://proxy.invalid/path",
                            "http://[not-an-ip]"})
    {
        CHECK_THROWS_AS(
            HttpClient::newHttpClientWithProxy("https://origin.invalid", url),
            std::invalid_argument);
    }
}
