#include <drogon/WebSocketClient.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/net/EventLoopThread.h>

#include <iostream>

using namespace drogon;
using namespace std::chrono_literals;

Task<> doTest(WebSocketClientPtr wsPtr, HttpRequestPtr req, bool continually)
{
    wsPtr->setAsyncMessageHandler(
        [continually](std::string&& message,
                      const WebSocketClientPtr wsPtr,
                      const WebSocketMessageType type) -> Task<> {
            std::cout << "new message:" << message << std::endl;
            if (type == WebSocketMessageType::Pong)
            {
                std::cout << "recv a pong" << std::endl;
                if (!continually)
                {
                    app().getLoop()->quit();
                }
            }
            co_return;
        });
    wsPtr->setAsyncConnectionClosedHandler(
        [](const WebSocketClientPtr wsPtr) -> Task<> {
            std::cout << "ws closed!" << std::endl;
            co_return;
        });

    try
    {
        auto resp = co_await wsPtr->connectToServerCoro(req);
    }
    catch (...)
    {
        std::cout << "ws failed!" << std::endl;
        if (!continually)
        {
            exit(1);
        }
    }
    std::cout << "ws connected!" << std::endl;
    wsPtr->getConnection()->setPingMessage("", 2s);
    wsPtr->getConnection()->send("hello!");
}

int main(int argc, char* argv[])
{
    auto wsPtr = WebSocketClient::newWebSocketClient("127.0.0.1", 8848);
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/chat");
    bool continually = true;
    if (argc > 1)
    {
        if (std::string(argv[1]) == "-t")
            continually = false;
        else if (std::string(argv[1]) == "-p")
        {
            // Connect to a public web socket server.
            wsPtr =
                WebSocketClient::newWebSocketClient("wss://echo.websocket.org");
            req->setPath("/");
        }
    }

    app().getLoop()->runAfter(5.0, [continually]() {
        if (!continually)
        {
            exit(1);
        }
    });
    app().setLogLevel(trantor::Logger::kTrace);

    auto test = [=]() -> AsyncTask {
        co_await doTest(wsPtr, req, continually);
    }();

    app().run();
}
