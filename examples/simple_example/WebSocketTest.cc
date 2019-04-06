#include "WebSocketTest.h"
using namespace example;
void WebSocketTest::handleNewMessage(const WebSocketConnectionPtr &wsConnPtr, std::string &&message, const WebSocketMessageType &type)
{
    //write your application logic here
    LOG_DEBUG << "new websocket message:" << message;
    if (type == WebSocketMessageType::Ping)
    {
        LOG_DEBUG << "recv a ping";
    }
}

void WebSocketTest::handleConnectionClosed(const WebSocketConnectionPtr &)
{
    LOG_DEBUG << "websocket closed!";
}

void WebSocketTest::handleNewConnection(const HttpRequestPtr &,
                                        const WebSocketConnectionPtr &conn)
{
    LOG_DEBUG << "new websocket connection!";
    conn->send("haha!!!");
}
