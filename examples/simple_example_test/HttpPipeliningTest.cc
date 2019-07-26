#include <drogon/HttpClient.h>
#include <drogon/HttpAppFramework.h>
#include <string>
#include <iostream>
#include <atomic>
using namespace drogon;
int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    client->setPipeliningDepth(64);
    int counter = -1;
    int n = 0;

    auto request1 = HttpRequest::newHttpRequest();
    request1->setPath("/pipe");
    request1->setMethod(Head);

    client->sendRequest(
        request1, [&counter, &n](ReqResult r, const HttpResponsePtr &resp) {
            if (r == ReqResult::Ok)
            {
                auto counterHeader = resp->getHeader("counter");
                int c = atoi(counterHeader.data());
                if (c <= counter)
                {
                    LOG_ERROR << "The response was received in "
                                 "the wrong order!";
                    exit(1);
                }
                else
                {
                    counter = c;
                    n++;
                }
                if (resp->getBody().length() > 0)
                {
                    LOG_ERROR << "The response has a body:" << resp->getBody();
                    exit(1);
                }
            }
            else
            {
                exit(1);
            }
        });

    auto request2 = HttpRequest::newHttpRequest();
    request2->setPath("/drogon.jpg");
    client->sendRequest(request2, [](ReqResult r, const HttpResponsePtr &resp) {
        if (r == ReqResult::Ok)
        {
            if (resp->getBody().length() != 44618)
            {
                LOG_ERROR << "The response is error!";
                exit(1);
            }
        }
        else
        {
            exit(1);
        }
    });

    auto request = HttpRequest::newHttpRequest();
    request->setPath("/pipe");
    for (int i = 0; i < 19; i++)
    {
        client->sendRequest(
            request, [&counter, &n](ReqResult r, const HttpResponsePtr &resp) {
                if (r == ReqResult::Ok)
                {
                    auto counterHeader = resp->getHeader("counter");
                    int c = atoi(counterHeader.data());
                    if (c <= counter)
                    {
                        LOG_ERROR
                            << "The response was received in the wrong order!";
                        exit(1);
                    }
                    else
                    {
                        counter = c;
                        n++;
                        if (n == 20)
                        {
                            LOG_DEBUG << "Good!";
                            app().getLoop()->quit();
                        }
                    }
                    if (resp->getBody().length() == 0)
                    {
                        LOG_ERROR << "The response hasn't a body!";
                        exit(1);
                    }
                }
                else
                {
                    exit(1);
                }
            });
    }
    app().run();
}
