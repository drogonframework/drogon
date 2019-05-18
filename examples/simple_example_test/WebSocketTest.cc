#include <drogon/WebSocketClient.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/net/EventLoopThread.h>

#include <iostream>

using namespace drogon;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    auto wsPtr = WebSocketClient::newWebSocketClient("127.0.0.1", 8848);
    auto req = HttpRequest::newHttpRequest();
    bool continually = true;
    if (argc > 1)
    {
        if (std::string(argv[1]) == "-t")
            continually = false;
    }
    req->setPath("/chat");
    wsPtr->setMessageHandler([continually](const std::string &message,
                                           const WebSocketClientPtr &wsPtr,
                                           const WebSocketMessageType &type) {
        std::cout << "new message:" << message << std::endl;
        if (type == WebSocketMessageType::Pong)
        {
            std::cout << "recv a pong" << std::endl;
            if (!continually)
            {
                app().getLoop()->quit();
            }
        }
    });
    wsPtr->setConnectionClosedHandler([](const WebSocketClientPtr &wsPtr) {
        std::cout << "ws closed!" << std::endl;
    });
    wsPtr->connectToServer(req,
                           [continually](ReqResult r,
                                         const HttpResponsePtr &resp,
                                         const WebSocketClientPtr &wsPtr) {
                               if (r == ReqResult::Ok)
                               {
                                   std::cout << "ws connected!" << std::endl;
                                   wsPtr->getConnection()->setPingMessage("",
                                                                          2s);
                                   wsPtr->getConnection()->send("hello!");
                               }
                               else
                               {
                                   std::cout << "ws failed!" << std::endl;
                                   if (!continually)
                                   {
                                       exit(-1);
                                   }
                               }
                           });
    app().getLoop()->runAfter(5.0, [continually]() {
        if (!continually)
        {
            exit(-1);
        }
    });
    app().run();
}