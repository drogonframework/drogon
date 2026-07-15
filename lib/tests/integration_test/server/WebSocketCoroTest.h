#pragma once

#include <drogon/WebSocketController.h>
#include <atomic>

namespace example
{
class WebSocketCoroTest
    : public drogon::WebSocketCoroController<WebSocketCoroTest>
{
  public:
    drogon::Task<> handleNewMessageCoro(
        const drogon::WebSocketConnectionPtr &,
        std::string &&,
        const drogon::WebSocketMessageType &) override;
    drogon::Task<> handleConnectionClosedCoro(
        const drogon::WebSocketConnectionPtr &) override;
    drogon::Task<> handleNewConnectionCoro(
        const drogon::HttpRequestPtr &,
        const drogon::WebSocketConnectionPtr &) override;

    WS_CORO_PATH_LIST_BEGIN
    WS_CORO_PATH_ADD("/coro-chat", drogon::Get);
    WS_CORO_PATH_LIST_END

  private:
    static std::atomic<int> openedCount_;
    static std::atomic<int> messageCount_;
    static std::atomic<int> closedCount_;
};
}  // namespace example
