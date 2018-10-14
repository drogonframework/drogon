#pragma once
#include <drogon/WebSocketController.h>
using namespace drogon;
namespace example
{
class WebSocketTest : public drogon::WebSocketController<WebSocketTest>
{
  public:
    virtual void handleNewMessage(const WebSocketConnectionPtr &,
                                  std::string &&) override;
    virtual void handleConnectionClosed(const WebSocketConnectionPtr &) override;
    virtual void handleNewConnection(const HttpRequestPtr &, const WebSocketConnectionPtr &) override;
    WS_PATH_LIST_BEGIN
    //list path definations here;
    WS_PATH_ADD("/chat");
    WS_PATH_LIST_END
};
} // namespace example
