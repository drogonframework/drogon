#include <drogon/WebSocketClient.h>
#include <drogon/HttpAppFramework.h>

#include <iostream>

using namespace drogon;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    std::string server;
    std::string path;
    optional<uint16_t> port;
    // Connect to a public echo server
    if (argc > 1 && std::string(argv[1]) == "-p")
    {
        server = "wss://echo.websocket.org";
        path = "/";
    }
    else
    {
        server = "ws://127.0.0.1";
        port = 8848;
        path = "/chat";
    }

    WebSocketClientPtr wsPtr;
    if (port.value_or(0) != 0)
        wsPtr = WebSocketClient::newWebSocketClient(server, port.value());
    else
        wsPtr = WebSocketClient::newWebSocketClient(server);
    auto req = HttpRequest::newHttpRequest();
    req->setPath(path);

    wsPtr->setMessageHandler([](const std::string &message,
                                const WebSocketClientPtr &wsPtr,
                                const WebSocketMessageType &type) {
        std::string messageType = "Unknown";
        if (type == WebSocketMessageType::Text)
            messageType = "text";
        else if (type == WebSocketMessageType::Pong)
            messageType = "pong";
        else if (type == WebSocketMessageType::Ping)
            messageType = "ping";
        else if (type == WebSocketMessageType::Binary)
            messageType = "binary";
        else if (type == WebSocketMessageType::Close)
            messageType = "Close";

        LOG_INFO << "new message (" << messageType << "): " << message;
    });

    wsPtr->setConnectionClosedHandler([](const WebSocketClientPtr &wsPtr) {
        LOG_INFO << "WebSocket connection closed!";
    });

    LOG_INFO << "Connecting to WebSocket at " << server;
    wsPtr->connectToServer(
        req,
        [](ReqResult r,
           const HttpResponsePtr &resp,
           const WebSocketClientPtr &wsPtr) {
            if (r != ReqResult::Ok)
            {
                LOG_ERROR << "Failed to establish WebSocket connection!";
                exit(1);
            }
            LOG_INFO << "WebSocket connected!";
            wsPtr->getConnection()->setPingMessage("", 2s);
            wsPtr->getConnection()->send("hello!");
        });

    // Quit the application after 15 seconds
    app().getLoop()->runAfter(15, []() { app().quit(); });

    app().setLogLevel(trantor::Logger::kDebug);
    app().run();
    LOG_INFO << "bye!";
    return 0;
}