#include <drogon/HttpClient.h>
#include <drogon/drogon_test.h>
#include <trantor/net/TcpClient.h>

#include <future>
#include <string>
#include <string_view>

using namespace drogon;

namespace
{
std::string sendRawRequest(trantor::EventLoop *loop, std::string_view request)
{
    auto client = std::make_shared<trantor::TcpClient>(
        loop, trantor::InetAddress{"127.0.0.1", 8848}, "expect-continue-test");
    auto response = std::make_shared<std::string>();
    std::promise<void> disconnected;

    client->setMessageCallback([response](const trantor::TcpConnectionPtr &,
                                          trantor::MsgBuffer *buffer) {
        response->append(buffer->read(buffer->readableBytes()));
    });
    client->setConnectionCallback(
        [request, &disconnected](const trantor::TcpConnectionPtr &connection) {
            if (connection->connected())
            {
                connection->send(request.data(), request.size());
                connection->shutdown();
            }
            else
            {
                disconnected.set_value();
            }
        });

    client->connect();
    disconnected.get_future().wait();
    return *response;
}
}  // namespace

DROGON_TEST(EmptyBodyWithExpectContinue)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    const auto response =
        sendRawRequest(client->getLoop(),
                       "PUT /api/v1/apitest/static HTTP/1.1\r\n"
                       "Host: localhost\r\n"
                       "Content-Length: 0\r\n"
                       "Expect: 100-continue\r\n"
                       "Connection: close\r\n\r\n");

    CHECK(response.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
}

DROGON_TEST(NonEmptyBodyWithExpectContinue)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    const auto response =
        sendRawRequest(client->getLoop(),
                       "PUT /api/v1/apitest/static HTTP/1.1\r\n"
                       "Host: localhost\r\n"
                       "Content-Length: 1\r\n"
                       "Expect: 100-continue\r\n"
                       "Connection: close\r\n\r\n"
                       "x");

    CHECK(response.rfind("HTTP/1.1 100 Continue\r\n", 0) == 0);
    CHECK(response.find("HTTP/1.1 200 OK\r\n") != std::string::npos);
}
