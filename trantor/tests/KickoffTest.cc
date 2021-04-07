#include <trantor/net/TcpServer.h>
#include <trantor/utils/Logger.h>
#include <trantor/net/EventLoopThread.h>
#include <string>
#include <iostream>
using namespace trantor;
#define USE_IPV6 0
int main()
{
    LOG_DEBUG << "test start";
    Logger::setLogLevel(Logger::kTrace);
    EventLoop loop;
#if USE_IPV6
    InetAddress addr(8888, true, true);
#else
    InetAddress addr(8888);
#endif
    TcpServer server(&loop, addr, "test");
    server.kickoffIdleConnections(10);
    server.setRecvMessageCallback(
        [](const TcpConnectionPtr &connectionPtr, MsgBuffer *buffer) {
            // LOG_DEBUG<<"recv callback!";
            std::cout << std::string(buffer->peek(), buffer->readableBytes());
            connectionPtr->send(buffer->peek(), buffer->readableBytes());
            buffer->retrieveAll();
        });
    int n = 0;
    server.setConnectionCallback([&n](const TcpConnectionPtr &connPtr) {
        if (connPtr->connected())
        {
            ++n;
            if (n % 2 == 0)
            {
                connPtr->keepAlive();
            }
            LOG_DEBUG << "New connection";
        }
        else if (connPtr->disconnected())
        {
            LOG_DEBUG << "connection disconnected";
        }
    });
    server.setIoLoopNum(3);
    server.start();
    loop.loop();
}
