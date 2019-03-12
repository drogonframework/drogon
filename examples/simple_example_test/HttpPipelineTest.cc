#include <trantor/net/TcpClient.h>
#include <trantor/utils/Logger.h>
#include <trantor/net/EventLoopThread.h>
#include <string>
#include <iostream>
#include <atomic>
using namespace trantor;
int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    LOG_DEBUG << "TcpClient class test!";
    EventLoop loop;
    InetAddress serverAddr("127.0.0.1", 8848);
    trantor::TcpClient client(&loop, serverAddr, "tcpclienttest");
    client.setConnectionCallback([=, &loop](const TcpConnectionPtr &conn) {
        if (conn->connected())
        {
            std::string str = "GET /pipe HTTP/1.1\r\n"
                              "\r\n";
            for (int i = 0; i < 32; i++)
                conn->send(str);
        }
        else
        {
            loop.quit();
        }
    });
    client.setMessageCallback([=](const TcpConnectionPtr &conn,
                                  MsgBuffer *buf) {
        LOG_DEBUG << string_view(buf->peek(), buf->readableBytes());
        buf->retrieveAll();
    });
    client.connect();

    loop.loop();
}
