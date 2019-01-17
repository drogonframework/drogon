#include "WebSocketTest.h"
using namespace example;
void WebSocketTest::handleNewMessage(const WebSocketConnectionPtr &wsConnPtr, std::string &&message)
{
    //write your application logic here
    LOG_DEBUG << "new websocket message:" << message;
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
