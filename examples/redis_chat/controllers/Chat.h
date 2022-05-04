#pragma once

#include <drogon/WebSocketController.h>

using namespace drogon;

class Chat : public drogon::WebSocketController<Chat>
{
  public:
    virtual void handleNewMessage(const WebSocketConnectionPtr &,
                                  std::string &&,
                                  const WebSocketMessageType &) override;
    virtual void handleNewConnection(const HttpRequestPtr &,
                                     const WebSocketConnectionPtr &) override;
    virtual void handleConnectionClosed(
        const WebSocketConnectionPtr &) override;
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/chat");
    WS_PATH_LIST_END
};
