#pragma once
#include <drogon/WebSocketController.h>
#include <drogon/PubSubService.h>
using namespace drogon;
namespace example
{
class WebSocketTest : public drogon::WebSocketController<WebSocketTest>
{
  public:
    virtual void handleNewMessage(const WebSocketConnectionPtr &,
                                  std::string &&,
                                  const WebSocketMessageType &) override;
    virtual void handleConnectionClosed(
        const WebSocketConnectionPtr &) override;
    virtual void handleNewConnection(const HttpRequestPtr &,
                                     const WebSocketConnectionPtr &) override;
    WS_PATH_LIST_BEGIN
    // list path definations here;
    WS_PATH_ADD("/chat", "drogon::LocalHostFilter", Get);
    WS_PATH_LIST_END
  private:
    PubSubService<std::string> chatRooms_;
};
}  // namespace example
