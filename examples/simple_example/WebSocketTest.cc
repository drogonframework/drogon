#include "WebSocketTest.h"
using namespace example;
void WebSocketTest::handleNewMessage(const WebSocketConnectionPtr &wsConnPtr, std::string &&message)
{
    //write your application logic here
    LOG_TRACE << "new websocket message:" << message;
}
void WebSocketTest::handleConnectionClosed(const WebSocketConnectionPtr &)
{
    LOG_TRACE << "websocket closed!";
}
void WebSocketTest::handleNewConnection(const HttpRequestPtr &,
                                        const WebSocketConnectionPtr &)
{
    LOG_TRACE << "new websocket connection!";
}
