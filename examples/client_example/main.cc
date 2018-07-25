#include <drogon/HttpAppFramework.h>
#include <trantor/net/InetAddress.h>
#include <netdb.h>
#include <iostream>
#include <future>
int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    struct hostent* he = gethostbyname("www.baidu.com");
    struct in_addr addr;

    if (he && he->h_addrtype == AF_INET) {
        addr = *reinterpret_cast<struct in_addr*>(he->h_addr);
        struct sockaddr_in ad;
        bzero(&ad, sizeof ad);
        ad.sin_family = AF_INET;
        ad.sin_addr=addr;
        trantor::InetAddress netaddr(ad);
        auto client=HttpAppFramework::instance().newHttpClient(netaddr.toIp(),80);
        auto req=HttpRequest::newHttpRequest();
        req->setMethod(drogon::HttpRequest::kGet);
        int count=0;
        for(int i=0;i<10;i++)
            client->sendRequest(req,[&](ReqResult result,const HttpResponse &response){
                std::cout<<"receive response!"<<std::endl;
                //auto headers=response.
                count++;
                std::cout<<response.getBody()<<std::endl;
                std::cout<<"count="<<count<<std::endl;
            });
        HttpAppFramework::instance().run();
    } else {
        std::cout<<"can't find the host"<<std::endl;
    }
}