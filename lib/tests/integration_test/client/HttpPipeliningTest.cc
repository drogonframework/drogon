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

    auto request = HttpRequest::newHttpRequest();
    request->setPath("/pipe");
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
