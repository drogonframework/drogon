#include <drogon/WebSocketClient.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/net/EventLoopThread.h>

#include <iostream>
#include <array>

using namespace drogon;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    std::array<WebSocketClientPtr, 1000> wsClients;
    for (size_t i = 0; i < wsClients.size(); i++)
    {
        auto &wsPtr = wsClients[i];
        wsPtr = WebSocketClient::newWebSocketClient("127.0.0.1", 8848);
        auto req = HttpRequest::newHttpRequest();

        req->setPath("/chat");
        wsPtr->setMessageHandler([](const std::string &message,
                                    const WebSocketClientPtr &wsPtr,
                                    const WebSocketMessageType &type) {
            std::cout << "new message:" << message << std::endl;
            if (type == WebSocketMessageType::Pong)
            {
                std::cout << "recv a pong" << std::endl;
            }
        });
        wsPtr->setConnectionClosedHandler([](const WebSocketClientPtr &wsPtr) {
            std::cout << "ws closed!" << std::endl;
        });
        wsPtr->connectToServer(
            req,
            [](ReqResult r,
               const HttpResponsePtr &resp,
               const WebSocketClientPtr &wsPtr) {
                if (r == ReqResult::Ok)
                {
                    std::cout << "ws connected!" << std::endl;
                    wsPtr->getConnection()->setPingMessage("", 2s);
                    wsPtr->getConnection()->send("hello!");
                }
                else
                {
                    std::cout << "ws failed!" << std::endl;
                }
            });
    }
    app().run();
}