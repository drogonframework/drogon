#include <trantor/net/TcpClient.h>
#include <trantor/utils/Logger.h>
#include <trantor/net/EventLoopThread.h>
#include <string>
#include <iostream>
#include <atomic>
using namespace trantor;
#define USE_IPV6 0
int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::kTrace);
    LOG_DEBUG << "TcpClient class test!";
    EventLoop loop;
#if USE_IPV6
    InetAddress serverAddr("::1", 8888, true);
#else
    InetAddress serverAddr("127.0.0.1", 8888);
#endif
    std::shared_ptr<trantor::TcpClient> client[10];
    std::atomic_int connCount;
    connCount = 10;
    for (int i = 0; i < 10; ++i)
    {
        client[i] = std::make_shared<trantor::TcpClient>(&loop,
                                                         serverAddr,
                                                         "tcpclienttest");
        client[i]->setConnectionCallback(
            [i, &loop, &connCount](const TcpConnectionPtr &conn) {
                if (conn->connected())
                {
                    LOG_DEBUG << i << " connected!";
                    char tmp[20];
                    sprintf(tmp, "%d client!!", i);
                    conn->send(tmp);
                }
                else
                {
                    LOG_DEBUG << i << " disconnected";
                    --connCount;
                    if (connCount == 0)
                        loop.quit();
                }
            });
        client[i]->setMessageCallback(
            [](const TcpConnectionPtr &conn, MsgBuffer *buf) {
                LOG_DEBUG << std::string(buf->peek(), buf->readableBytes());
                buf->retrieveAll();
                conn->shutdown();
            });
        client[i]->connect();
    }
    loop.loop();
}
