#include <drogon/drogon_test.h>
#include <drogon/HttpClient.h>
#include <atomic>
#include <chrono>
#include <thread>

using namespace drogon;

DROGON_TEST(HttpClientOutstandingRequests)
{
    auto client = HttpClient::newHttpClient("http://127.0.0.1:8848");

    // Enable pipelining to allow multiple in-flight requests simultaneously
    client->setPipeliningDepth(64);

    CHECK(client->requestsBufferSize() == 0);
    CHECK(client->outstandingRequests() == 0);

    const int totalRequests = 50;
    std::atomic<int> completedRequests{0};

    for (int i = 0; i < totalRequests; ++i)
    {
        auto req = HttpRequest::newHttpRequest();
        req->setPath("/PipeliningTest/normalPipe");

        client->sendRequest(
            req,
            [&completedRequests](ReqResult result,
                                 const HttpResponsePtr &response) {
                if (result == ReqResult::Ok)
                {
                    completedRequests++;
                }
            });
    }

    size_t currentOutstanding = client->outstandingRequests();
    size_t currentBuffer = client->requestsBufferSize();

    CHECK(currentOutstanding > 0);
    CHECK(currentOutstanding >= currentBuffer);

    while (completedRequests < totalRequests)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CHECK(client->requestsBufferSize() == 0);
    CHECK(client->outstandingRequests() == 0);
}

DROGON_TEST(HttpClientOutstandingRequests_ConnectionError)
{
    // Point to an unassigned port that immediately refuses connection
    auto client = HttpClient::newHttpClient("http://127.0.0.1:65534");
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/");

    const int totalRequests = 10;
    std::atomic<int> completedRequests{0};

    for (int i = 0; i < totalRequests; ++i)
    {
        client->sendRequest(
            req,
            [&completedRequests](ReqResult result,
                                 const HttpResponsePtr &response) {
                if (result != ReqResult::Ok)
                {
                    completedRequests++;
                }
            });
    }

    while (completedRequests < totalRequests)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CHECK(completedRequests == totalRequests);
    CHECK(client->requestsBufferSize() == 0);
    CHECK(client->outstandingRequests() == 0);
}

DROGON_TEST(HttpClientOutstandingRequests_BadAddress)
{
    // Invalid IP triggers the DNS/address validation failure path
    auto client = HttpClient::newHttpClient("http://256.256.256.256");
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/");

    const int totalRequests = 5;
    std::atomic<int> completedRequests{0};

    for (int i = 0; i < totalRequests; ++i)
    {
        client->sendRequest(
            req,
            [&completedRequests](ReqResult result,
                                 const HttpResponsePtr &response) {
                if (result == ReqResult::BadServerAddress)
                {
                    completedRequests++;
                }
            });
    }

    while (completedRequests < totalRequests)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CHECK(completedRequests == totalRequests);
    CHECK(client->requestsBufferSize() == 0);
    CHECK(client->outstandingRequests() == 0);
}

DROGON_TEST(HttpClientOutstandingRequests_Timeout)
{
    // 192.0.2.1 (TEST-NET-1) drops packets to force request expiration
    auto client = HttpClient::newHttpClient("http://192.0.2.1");
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/");

    const int totalRequests = 5;
    std::atomic<int> completedRequests{0};

    for (int i = 0; i < totalRequests; ++i)
    {
        client->sendRequest(
            req,
            [&completedRequests](ReqResult result,
                                 const HttpResponsePtr &response) {
                if (result == ReqResult::Timeout)
                {
                    completedRequests++;
                }
            },
            0.2);
    }

    while (completedRequests < totalRequests)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CHECK(completedRequests == totalRequests);
    CHECK(client->requestsBufferSize() == 0);
    CHECK(client->outstandingRequests() == 0);
}
