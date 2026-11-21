#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/drogon_test.h>
#include <drogon/utils/Utilities.h>
#include <trantor/net/TcpServer.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

using namespace drogon;

namespace
{
using Response = std::pair<ReqResult, HttpResponsePtr>;

struct ReceivedRequest
{
    trantor::TcpConnectionPtr connection;
    std::string headers;
};

// Responses are released by the test, rather than by timing the server.
class TimeoutTestServer
{
  public:
    explicit TimeoutTestServer(bool readRawData = false)
    {
        std::promise<void> ready;
        app().getLoop()->queueInLoop([this, &ready, readRawData]() {
            server_ = std::make_unique<trantor::TcpServer>(
                app().getLoop(),
                trantor::InetAddress("127.0.0.1", 0),
                "http-client-timeout-test");
            port_ = server_->address().toPort();
            server_->setRecvMessageCallback([this, readRawData](
                                                const trantor::TcpConnectionPtr
                                                    &connection,
                                                trantor::MsgBuffer *buffer) {
                while (buffer->readableBytes() != 0)
                {
                    const std::string data(buffer->peek(),
                                           buffer->readableBytes());
                    const auto end = data.find("\r\n\r\n");
                    if (!readRawData && end == std::string::npos)
                        return;
                    const auto size = readRawData ? data.size() : end + 4;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        requests_.push_back({connection, data.substr(0, size)});
                    }
                    buffer->retrieve(size);
                    received_.notify_one();
                }
            });
            server_->start();
            ready.set_value();
        });
        ready.get_future().get();
    }

    ~TimeoutTestServer()
    {
        std::promise<void> stopped;
        app().getLoop()->queueInLoop([this, &stopped]() {
            server_->stop();
            server_.reset();
            stopped.set_value();
        });
        stopped.get_future().get();
    }

    HttpClientPtr client() const
    {
        return HttpClient::newHttpClient("127.0.0.1", port_);
    }

    HttpClientPtr client(HttpClient::Timeouts timeouts,
                         bool useSSL = false) const
    {
        return HttpClient::newHttpClient("127.0.0.1", port_, timeouts, useSSL);
    }

    std::string url() const
    {
        return "http://127.0.0.1:" + std::to_string(port_);
    }

    ReceivedRequest receive()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!received_.wait_for(lock, std::chrono::seconds(5), [this]() {
                return !requests_.empty();
            }))
        {
            throw std::runtime_error("local HTTP request did not arrive");
        }
        auto request = std::move(requests_.front());
        requests_.pop_front();
        return request;
    }

    void respond(const ReceivedRequest &request, const std::string &body)
    {
        std::string response = "HTTP/1.1 200 OK\r\nContent-Length: " +
                               std::to_string(body.size()) +
                               "\r\nConnection: keep-alive\r\n\r\n";
        if (request.headers.find("HEAD ") != 0)
            response += body;
        request.connection->send(response);
    }

  private:
    std::unique_ptr<trantor::TcpServer> server_;
    uint16_t port_{0};
    std::mutex mutex_;
    std::condition_variable received_;
    std::deque<ReceivedRequest> requests_;
};

HttpRequestPtr requestFor(const std::string &path, HttpMethod method = Get)
{
    auto request = HttpRequest::newHttpRequest();
    request->setPath(path);
    request->setMethod(method);
    return request;
}

std::future<Response> send(const HttpClientPtr &client,
                           const HttpRequestPtr &request,
                           double timeout,
                           std::shared_ptr<std::atomic<size_t>> calls = nullptr)
{
    auto completed = std::make_shared<std::promise<Response>>();
    auto future = completed->get_future();
    client->sendRequest(
        request,
        [completed, calls](ReqResult result, const HttpResponsePtr &response) {
            if (!calls || ++*calls == 1)
                completed->set_value({result, response});
        },
        timeout);
    return future;
}

std::future<Response> send(const HttpClientPtr &client,
                           const std::string &path,
                           double timeout = 5.0)
{
    return send(client, requestFor(path), timeout);
}

// Wait until the current callback and its timeout handler have returned.
void drainLoop()
{
    std::promise<void> drained;
    app().getLoop()->queueInLoop([&drained]() { drained.set_value(); });
    drained.get_future().get();
}
}  // namespace

DROGON_TEST(HttpClientTimeoutReleasesClientAndCallback)
{
    for (const bool reuseConnection : {false, true})
    {
        TimeoutTestServer server;
        auto client = server.client();
        if (reuseConnection)
        {
            auto warmup = send(client, "/warmup");
            server.respond(server.receive(), "warmup");
            REQUIRE(warmup.get().first == ReqResult::Ok);
        }

        const std::weak_ptr<HttpClient> weakClient = client;
        auto captured = std::make_shared<int>(42);
        const std::weak_ptr<int> weakCaptured = captured;
        std::atomic<size_t> callbacks{0};
        std::promise<void> completed;
        client->sendRequest(
            requestFor("/silent"),
            [TEST_CTX, client, captured, &callbacks, &completed](
                ReqResult result, const HttpResponsePtr &response) {
                CHECK(result == ReqResult::Timeout);
                CHECK(response == nullptr);
                CHECK(*captured == 42);
                CHECK(client != nullptr);
                if (++callbacks == 1)
                    completed.set_value();
            },
            0.1);
        client.reset();
        captured.reset();
        const auto pending = server.receive();
        completed.get_future().get();
        drainLoop();
        CHECK(weakCaptured.expired());
        CHECK(weakClient.expired());
        CHECK(callbacks == 1);
        // Also clean up the unfixed implementation after a failed assertion.
        server.respond(pending, "late");
    }
}

DROGON_TEST(HttpClientTimeoutPreservesLateHeadResponseAndReentrancy)
{
    TimeoutTestServer server;
    auto client = server.client();
    auto captured = std::make_shared<int>(42);
    const std::weak_ptr<int> weakCaptured = captured;
    std::atomic<size_t> callbacks{0};
    std::promise<void> timedOut;
    std::promise<Response> completed;
    auto request = requestFor("/late-head", Head);
    client->sendRequest(
        request,
        [TEST_CTX,
         client,
         request,
         captured,
         &callbacks,
         &timedOut,
         &completed](ReqResult result, const HttpResponsePtr &response) {
            CHECK(result == ReqResult::Timeout);
            CHECK(response == nullptr);
            if (++callbacks != 1)
                return;
            // The response parser must use the method that was sent.
            request->setMethod(Get);
            request->setPath("/next");
            client->sendRequest(
                request,
                [&completed](ReqResult result,
                             const HttpResponsePtr &response) {
                    completed.set_value({result, response});
                },
                5.0);
            timedOut.set_value();
        },
        0.1);
    captured.reset();
    const auto first = server.receive();
    CHECK(first.headers.find("HEAD /late-head ") == 0);
    timedOut.get_future().get();
    drainLoop();
    CHECK(weakCaptured.expired());
    CHECK(client->requestsBufferSize() == 1);
    // The timed-out HEAD still occupies its response-ordering slot.
    CHECK(client->outstandingRequests() == 2);

    // HEAD advertises a content length but has no response body.
    server.respond(first, "ignored");
    const auto second = server.receive();
    CHECK(second.connection == first.connection);
    CHECK(second.headers.find("GET /next ") == 0);
    server.respond(second, "second");
    const auto response = completed.get_future().get();
    REQUIRE(response.first == ReqResult::Ok);
    CHECK(response.second->body() == "second");
    CHECK(callbacks == 1);
    CHECK(client->outstandingRequests() == 0);
}

DROGON_TEST(HttpClientTimeoutPreservesPipelinedResponseOrder)
{
    for (const bool timeoutFirst : {false, true})
    {
        TimeoutTestServer server;
        auto client = server.client();
        client->setPipeliningDepth(2);
        auto first = send(client, "/first", timeoutFirst ? 0.1 : 5.0);
        auto second = send(client, "/second", timeoutFirst ? 5.0 : 0.1);
        auto third = send(client, "/third");
        const auto firstRequest = server.receive();
        const auto secondRequest = server.receive();
        const auto thirdRequest = server.receive();
        CHECK(firstRequest.headers.find("GET /first ") == 0);
        CHECK(secondRequest.headers.find("GET /second ") == 0);
        CHECK(thirdRequest.headers.find("GET /third ") == 0);
        CHECK(firstRequest.connection == thirdRequest.connection);

        auto &expired = timeoutFirst ? first : second;
        const auto timeout = expired.get();
        CHECK(timeout.first == ReqResult::Timeout);
        CHECK(timeout.second == nullptr);
        CHECK(client->outstandingRequests() == 3);
        server.respond(firstRequest, "first");
        server.respond(secondRequest, "second");
        server.respond(thirdRequest, "third");
        auto &remaining = timeoutFirst ? second : first;
        const auto response = remaining.get();
        REQUIRE(response.first == ReqResult::Ok);
        CHECK(response.second->body() == (timeoutFirst ? "second" : "first"));
        const auto finalResponse = third.get();
        REQUIRE(finalResponse.first == ReqResult::Ok);
        CHECK(finalResponse.second->body() == "third");
        CHECK(client->outstandingRequests() == 0);
    }
}

DROGON_TEST(HttpClientTimeoutRemovesUnsentRequest)
{
    TimeoutTestServer server;
    auto client = server.client();
    auto first = send(client, "/first");
    const auto firstRequest = server.receive();
    auto expired = send(client, "/never-sent", 0.1);
    auto third = send(client, "/third");
    const auto timeout = expired.get();
    CHECK(timeout.first == ReqResult::Timeout);
    CHECK(timeout.second == nullptr);
    CHECK(client->requestsBufferSize() == 1);
    CHECK(client->outstandingRequests() == 2);
    server.respond(firstRequest, "first");
    REQUIRE(first.get().first == ReqResult::Ok);
    const auto nextRequest = server.receive();
    CHECK(nextRequest.headers.find("GET /third ") == 0);
    server.respond(nextRequest, "third");
    const auto response = third.get();
    REQUIRE(response.first == ReqResult::Ok);
    CHECK(response.second->body() == "third");
    CHECK(client->outstandingRequests() == 0);
}

DROGON_TEST(HttpClientWithoutTimeoutKeepsClientUntilResponse)
{
    TimeoutTestServer server;
    auto client = server.client();
    const std::weak_ptr<HttpClient> weakClient = client;
    auto completed = send(client, "/no-timeout", 0.0);
    client.reset();
    const auto pending = server.receive();
    CHECK(!weakClient.expired());
    server.respond(pending, "ok");
    const auto response = completed.get();
    REQUIRE(response.first == ReqResult::Ok);
    CHECK(response.second->body() == "ok");
    drainLoop();
    CHECK(weakClient.expired());
}

DROGON_TEST(HttpClientConnectTimeoutWhileAwaitingHandshake)
{
    if (!utils::supportsTls())
        return;

    // Read the ClientHello without completing TLS. TCP is connected, but the
    // client cannot dispatch an HTTP request yet.
    TimeoutTestServer server(true);
    auto client = server.client(HttpClient::Timeouts{0.2, 0.1}, true);
    for (const bool retry : {false, true})
    {
        auto pending = send(client, retry ? "/retry" : "/first");
        if (!retry)
        {
            const auto hello = server.receive();
            REQUIRE(static_cast<unsigned char>(hello.headers.front()) == 0x16);
        }
        const auto result = pending.get();
        CHECK(result.first == ReqResult::ConnectTimeout);
        CHECK(result.second == nullptr);
        CHECK(client->requestsBufferSize() == 0);
        CHECK(client->outstandingRequests() == 0);
    }
    CHECK(client->bytesSent() == 0);
}

DROGON_TEST(HttpClientConnectBudgetEndsAtDispatch)
{
    TimeoutTestServer server;
    auto client = server.client(HttpClient::Timeouts{0.2, 5.0});
    auto slow = send(client, "/slow");
    const auto first = server.receive();

    // With pipelining disabled, the queued request expires while the first
    // request stays in its response phase on the very same connection.
    auto queued = send(client, "/never-sent").get();
    CHECK(queued.first == ReqResult::ConnectTimeout);
    CHECK(queued.second == nullptr);
    CHECK(client->requestsBufferSize() == 0);
    CHECK(client->outstandingRequests() == 1);
    server.respond(first, "slow response");
    const auto result = slow.get();
    REQUIRE(result.first == ReqResult::Ok);
    CHECK(result.second->body() == "slow response");

    auto capped = send(client, "/total", 0.1);
    const auto reused = server.receive();
    CHECK(reused.connection == first.connection);
    CHECK(reused.headers.find("GET /total ") == 0);
    const auto timeout = capped.get();
    CHECK(timeout.first == ReqResult::Timeout);
    CHECK(timeout.second == nullptr);
    server.respond(reused, "late");
}

DROGON_TEST(HttpClientResponseTimeoutPreservesRepeatedRequests)
{
    TimeoutTestServer server;
    auto client = server.client(HttpClient::Timeouts{0, 0.2});
    auto request = requestFor("/repeat");
    auto calls = std::make_shared<std::atomic<size_t>>(0);
    auto first = send(client, request, 5.0, calls);
    const auto original = server.receive();

    // The same request object has three distinct submissions. The first
    // times out in flight, the last in the buffer on its shorter total budget.
    // The middle submission waits longer than its entire response budget.
    auto middle = send(client, request, 5.0);
    auto last = send(client, request, 0.5);
    const auto responseTimeout = first.get();
    CHECK(responseTimeout.first == ReqResult::ResponseTimeout);
    CHECK(responseTimeout.second == nullptr);
    const auto totalTimeout = last.get();
    CHECK(totalTimeout.first == ReqResult::Timeout);
    CHECK(totalTimeout.second == nullptr);
    CHECK(client->requestsBufferSize() == 1);
    CHECK(client->outstandingRequests() == 2);
    server.respond(original, "late");
    const auto repeated = server.receive();
    CHECK(repeated.connection == original.connection);
    server.respond(repeated, "middle");
    const auto response = middle.get();
    REQUIRE(response.first == ReqResult::Ok);
    CHECK(response.second->body() == "middle");
    CHECK(*calls == 1);
    CHECK(client->outstandingRequests() == 0);
}

DROGON_TEST(HttpClientResponseTimeoutWithPartialPipelinedResponse)
{
    TimeoutTestServer server;
    auto client =
        HttpClient::newHttpClient(server.url(), HttpClient::Timeouts{0, 0.5});
    client->setPipeliningDepth(1);
    auto warmup = send(client, "/warmup");
    const auto warm = server.receive();
    server.respond(warm, "warm");
    REQUIRE(warmup.get().first == ReqResult::Ok);

    auto calls = std::make_shared<std::atomic<size_t>>(0);
    auto first = send(client, requestFor("/partial"), 5.0, calls);
    const auto partial = server.receive();
    CHECK(partial.connection == warm.connection);
    partial.connection->send("HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nla");
    const auto timeout = first.get();
    CHECK(timeout.first == ReqResult::ResponseTimeout);
    CHECK(timeout.second == nullptr);
    CHECK(client->outstandingRequests() == 1);

    // Both requests are on the wire before the first response is complete.
    auto next = send(client, "/next");
    const auto second = server.receive();
    CHECK(second.connection == partial.connection);
    CHECK(second.headers.find("GET /next ") == 0);
    CHECK(client->outstandingRequests() == 2);
    partial.connection->send("te");
    server.respond(second, "next");
    const auto response = next.get();
    REQUIRE(response.first == ReqResult::Ok);
    CHECK(response.second->body() == "next");
    CHECK(*calls == 1);
    CHECK(client->outstandingRequests() == 0);
}
