#include "WebSocketTest.h"
using namespace example;
void WebSocketTest::handleNewMessage(const WebSocketConnectionPtr& wsConnPtr,const std::string &message)
{
    //write your application logic here
    LOG_TRACE<<"new websocket message:"<<message;
}void WebSocketTest::handleConnection(const WebSocketConnectionPtr& wsConnPtr) {
    //write your application logic here

    wsConnPtr->send("Hello!!");
}
