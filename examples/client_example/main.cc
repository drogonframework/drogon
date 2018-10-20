#include <drogon/drogon.h>
#include <iostream>
#include <future>
int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);

    auto client = HttpClient::newHttpClient("http://www.baidu.com");
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::HttpRequest::Get);
    req->setPath("/s");
    req->setParameter("wd", "weixin");
    int count = 0;
    for (int i = 0; i < 10; i++)
    {
        client->sendRequest(req, [&](ReqResult result, const HttpResponse &response) {
            std::cout << "receive response!" << std::endl;
            //auto headers=response.
            count++;
            std::cout << response.getBody() << std::endl;
            std::cout << "count=" << count << std::endl;
        });
    }

    HttpAppFramework::instance().run();
}
