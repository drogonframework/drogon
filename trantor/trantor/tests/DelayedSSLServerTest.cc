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
    EventLoopThread loopThread;
    loopThread.run();
#if USE_IPV6
    InetAddress addr(8888, true, true);
#else
    InetAddress addr(8888);
#endif
    TcpServer server(loopThread.getLoop(), addr, "test");
    auto ctx = newSSLServerContext("server.pem", "server.pem");
    LOG_INFO << "start";
    server.setRecvMessageCallback(
        [](const TcpConnectionPtr &connectionPtr, MsgBuffer *buffer) {
            LOG_DEBUG << std::string{buffer->peek(), buffer->readableBytes()};
            connectionPtr->send(*buffer);
            buffer->retrieveAll();
            connectionPtr->shutdown();
        });
    server.setConnectionCallback([ctx](const TcpConnectionPtr &connPtr) {
        if (connPtr->connected())
        {
            LOG_DEBUG << "New connection";
            connPtr->send("hello");
            connPtr->startServerEncryption(ctx, [] {
                LOG_INFO << "SSL established";
            });
        }
        else if (connPtr->disconnected())
        {
            LOG_DEBUG << "connection disconnected";
        }
    });
    server.setIoLoopNum(3);
    server.start();
    loopThread.wait();
}
