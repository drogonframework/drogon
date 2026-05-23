#include "WebSocketCoroTest.h"

#include <drogon/drogon.h>

using namespace example;

std::atomic<int> WebSocketCoroTest::openedCount_{0};
std::atomic<int> WebSocketCoroTest::messageCount_{0};
std::atomic<int> WebSocketCoroTest::closedCount_{0};

drogon::Task<> WebSocketCoroTest::handleNewConnectionCoro(
    const drogon::HttpRequestPtr &,
    const drogon::WebSocketConnectionPtr &conn)
{
    ++openedCount_;
    co_await drogon::sleepCoro(drogon::app().getLoop(), 0.001);
    conn->send("opened");
    co_return;
}

drogon::Task<> WebSocketCoroTest::handleNewMessageCoro(
    const drogon::WebSocketConnectionPtr &conn,
    std::string &&message,
    const drogon::WebSocketMessageType &type)
{
    if (type != drogon::WebSocketMessageType::Text)
    {
        co_return;
    }

    ++messageCount_;
    co_await drogon::sleepCoro(drogon::app().getLoop(), 0.001);

    if (message == "stats")
    {
        conn->send("stats:" + std::to_string(openedCount_.load()) + ":" +
                   std::to_string(messageCount_.load()) + ":" +
                   std::to_string(closedCount_.load()));
    }
    else
    {
        conn->send("coro:" + message);
    }
    co_return;
}

drogon::Task<> WebSocketCoroTest::handleConnectionClosedCoro(
    const drogon::WebSocketConnectionPtr &)
{
    ++closedCount_;
    co_await drogon::sleepCoro(drogon::app().getLoop(), 0.001);
    co_return;
}
