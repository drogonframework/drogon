#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/drogon_test.h>
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
    TimeoutTestServer()
    {
        std::promise<void> ready;
        app().getLoop()->queueInLoop([this, &ready]() {
            server_ = std::make_unique<trantor::TcpServer>(
                app().getLoop(),
                trantor::InetAddress("127.0.0.1", 0),
                "http-client-timeout-test");
            port_ = server_->address().toPort();
            server_->setRecvMessageCallback(
                [this](const trantor::TcpConnectionPtr &connection,
                       trantor::MsgBuffer *buffer) {
                    while (buffer->readableBytes() != 0)
                    {
                        const std::string data(buffer->peek(),
                                               buffer->readableBytes());
                        const auto end = data.find("\r\n\r\n");
                        if (end == std::string::npos)
                            return;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            requests_.push_back(
                                {connection, data.substr(0, end + 4)});
                        }
                        buffer->retrieve(end + 4);
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
                           const std::string &path,
                           double timeout = 5.0)
{
    auto completed = std::make_shared<std::promise<Response>>();
    auto future = completed->get_future();
    client->sendRequest(
        requestFor(path),
        [completed](ReqResult result, const HttpResponsePtr &response) {
            completed->set_value({result, response});
        },
        timeout);
    return future;
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
    client->sendRequest(
        requestFor("/late-head", Head),
        [TEST_CTX, client, captured, &callbacks, &timedOut, &completed](
            ReqResult result, const HttpResponsePtr &response) {
            CHECK(result == ReqResult::Timeout);
            CHECK(response == nullptr);
            if (++callbacks != 1)
                return;
            client->sendRequest(
                requestFor("/next"),
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
