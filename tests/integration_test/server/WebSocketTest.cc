#include "WebSocketTest.h"
using namespace example;
struct Subscriber
{
    std::string chatRoomName_;
    drogon::SubscriberID id_;
};
void WebSocketTest::handleNewMessage(const WebSocketConnectionPtr &wsConnPtr,
                                     std::string &&message,
                                     const WebSocketMessageType &type)
{
    // write your application logic here
    LOG_DEBUG << "new websocket message:" << message;
    if (type == WebSocketMessageType::Ping)
    {
        LOG_DEBUG << "recv a ping";
    }
    else if (type == WebSocketMessageType::Text)
    {
        auto &s = wsConnPtr->getContextRef<Subscriber>();
        chatRooms_.publish(s.chatRoomName_, message);
    }
}

void WebSocketTest::handleConnectionClosed(const WebSocketConnectionPtr &conn)
{
    LOG_DEBUG << "websocket closed!";
    auto &s = conn->getContextRef<Subscriber>();
    chatRooms_.unsubscribe(s.chatRoomName_, s.id_);
}

void WebSocketTest::handleNewConnection(const HttpRequestPtr &req,
                                        const WebSocketConnectionPtr &conn)
{
    LOG_DEBUG << "new websocket connection!";
    conn->send("haha!!!");
    Subscriber s;
    s.chatRoomName_ = req->getParameter("room_name");
    s.id_ = chatRooms_.subscribe(s.chatRoomName_,
                                 [conn](const std::string &topic,
                                        const std::string &message) {
                                     conn->send(message);
                                 });
    conn->setContext(std::make_shared<Subscriber>(std::move(s)));
}
