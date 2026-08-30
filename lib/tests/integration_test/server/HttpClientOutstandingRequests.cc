#include <drogon/drogon_test.h>
#include <drogon/HttpClient.h>
#include <atomic>
#include <thread>
#include <chrono>

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
    size_t currentBuffer = client->outstandingRequests();

    CHECK(currentOutstanding > 0);

    CHECK(currentOutstanding >= currentBuffer);

    while (completedRequests < totalRequests)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CHECK(client->requestsBufferSize() == 0);
    CHECK(client->outstandingRequests() == 0);
}
