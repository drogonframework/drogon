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
    auto request = HttpRequest::newHttpRequest();
    request->setPath("/pipe");
    int counter = -1;
    int n = 0;
    for (int i = 0; i < 20; i++)
    {
        client->sendRequest(request, [&counter, &n](ReqResult r, const HttpResponsePtr &resp) {
            if (r == ReqResult::Ok)
            {
                auto counterHeader = resp->getHeader("counter");
                int c = atoi(counterHeader.data());
                if (c <= counter)
                {
                    LOG_ERROR << "The response was received in the wrong order!";
                    exit(-1);
                }
                else
                {
                    counter = c;
                    n++;
                    if(n==20)
                    {
                        LOG_DEBUG << "Good!";
                        app().getLoop()->quit();
                    }
                }
            }
            else
            {
                exit(-1);
            }
            
        });
    }
    app().run();
}
