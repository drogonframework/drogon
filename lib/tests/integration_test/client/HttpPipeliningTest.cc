#include <drogon/HttpClient.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/drogon_test.h>
#include <string>
#include <iostream>
#include <atomic>
using namespace drogon;

static int counter = -1;

DROGON_TEST(HttpPipeliningTest)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    client->setPipeliningDepth(64);

    auto request1 = HttpRequest::newHttpRequest();
    request1->setPath("/pipe");
    request1->setMethod(Head);

    client->sendRequest(
        request1, [TEST_CTX](ReqResult r, const HttpResponsePtr &resp) {
            REQUIRE(r == ReqResult::Ok);
            auto counterHeader = resp->getHeader("counter");
            int c = atoi(counterHeader.data());
            if (c <= counter)
                FAIL("The response was received in the wrong order!");
            else
                SUCCESS();
            counter = c;
            REQUIRE(resp->body().empty());
        });

    auto request2 = HttpRequest::newHttpRequest();
    request2->setPath("/drogon.jpg");
    client->sendRequest(request2,
                        [TEST_CTX](ReqResult r, const HttpResponsePtr &resp) {
                            REQUIRE(r == ReqResult::Ok);
                            REQUIRE(resp->getBody().length() == 44618UL);
                        });

    for (int i = 0; i < 19; ++i)
    {
        client->sendRequest(
            request1, [TEST_CTX](ReqResult r, const HttpResponsePtr &resp) {
                REQUIRE(r == ReqResult::Ok);
                auto counterHeader = resp->getHeader("counter");
                int c = atoi(counterHeader.data());
                if (c <= counter)
                    FAIL("The response was received in the wrong order!");
                else
                    SUCCESS();
                counter = c;
                REQUIRE(resp->body().empty());
            });
    }
}

DROGON_TEST(HttpPipeliningStrangeTest1)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    client->setPipeliningDepth(64);
    for (int i = 0; i < 4; ++i)
    {
        auto request = HttpRequest::newHttpRequest();
        request->setPath("/pipe/strange-1");
        request->setBody(std::to_string(i));
        client->sendRequest(request,
                            [TEST_CTX, i](ReqResult r,
                                          const HttpResponsePtr &resp) {
                                REQUIRE(r == ReqResult::Ok);
                                REQUIRE(resp->body() == std::to_string(i));
                            });
    }
}

DROGON_TEST(HttpPipeliningStrangeTest2)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    client->setPipeliningDepth(64);
    for (int i = 0; i < 6; ++i)
    {
        auto request = HttpRequest::newHttpRequest();
        request->setPath("/pipe/strange-2");
        request->setBody(std::to_string(i));
        client->sendRequest(request,
                            [TEST_CTX, i](ReqResult r,
                                          const HttpResponsePtr &resp) {
                                REQUIRE(r == ReqResult::Ok);
                                REQUIRE(resp->body() == std::to_string(i));
                            });
    }
}
