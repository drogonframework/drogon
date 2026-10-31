#include <drogon/HttpClient.h>
#include <drogon/drogon_test.h>
#include <trantor/net/TcpClient.h>

#include <future>
#include <memory>
#include <string>
#include <string_view>

using namespace drogon;

namespace
{
std::string sendRawRequest(trantor::EventLoop *loop, std::string_view request)
{
    auto client =
        std::make_shared<trantor::TcpClient>(loop,
                                             trantor::InetAddress{"127.0.0.1",
                                                                  8848},
                                             "content-length-framing-test");
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

void expectStatus(const std::shared_ptr<test::Case> &TEST_CTX,
                  trantor::EventLoop *loop,
                  std::string_view request,
                  std::string_view status)
{
    const auto response = sendRawRequest(loop, request);
    if (response.rfind(status, 0) != 0)
    {
        FAIL("answered '",
             response.substr(0, response.find("\r\n")),
             "' instead of ",
             status);
        return;
    }
    SUCCESS();
}
}  // namespace

// RFC 9112 section 6.3: a repeated value that differs from the first one
// makes the framing ambiguous, so the message must be rejected.
DROGON_TEST(RejectConflictingContentLength)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    expectStatus(TEST_CTX,
                 client->getLoop(),
                 "PUT /api/v1/apitest/static HTTP/1.1\r\n"
                 "Host: localhost\r\n"
                 "Content-Length: 0\r\n"
                 "Content-Length: 1\r\n"
                 "Connection: close\r\n\r\n"
                 "x",
                 "HTTP/1.1 400");
}

// RFC 9110 section 8.6: Content-Length = 1*DIGIT, so an empty value is not
// "no Content-Length"; it is invalid framing.
DROGON_TEST(RejectEmptyContentLength)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    expectStatus(TEST_CTX,
                 client->getLoop(),
                 "PUT /api/v1/apitest/static HTTP/1.1\r\n"
                 "Host: localhost\r\n"
                 "Content-Length:\r\n"
                 "Connection: close\r\n\r\n",
                 "HTTP/1.1 400");
}

// RFC 9112 section 5.1: whitespace between a field name and the colon must
// be rejected with 400.
DROGON_TEST(RejectWhitespaceBeforeColon)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    expectStatus(TEST_CTX,
                 client->getLoop(),
                 "PUT /api/v1/apitest/static HTTP/1.1\r\n"
                 "Host: localhost\r\n"
                 "Content-Length : 1\r\n"
                 "Connection: close\r\n\r\n"
                 "x",
                 "HTTP/1.1 400");
}

// A comma-separated list is not 1*DIGIT. RFC 9112 section 6.3 would also
// allow folding identical members, but rejecting every list is simpler and
// equally compliant.
DROGON_TEST(RejectContentLengthList)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    expectStatus(TEST_CTX,
                 client->getLoop(),
                 "PUT /api/v1/apitest/static HTTP/1.1\r\n"
                 "Host: localhost\r\n"
                 "Content-Length: 1, 1\r\n"
                 "Connection: close\r\n\r\n"
                 "x",
                 "HTTP/1.1 400");
}

// A value beyond the size_t range cannot be framed and must be rejected.
DROGON_TEST(RejectOverflowingContentLength)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    expectStatus(TEST_CTX,
                 client->getLoop(),
                 "PUT /api/v1/apitest/static HTTP/1.1\r\n"
                 "Host: localhost\r\n"
                 "Content-Length: 99999999999999999999999\r\n"
                 "Connection: close\r\n\r\n",
                 "HTTP/1.1 400");
}

// Repeated fields with the same value are redundant but unambiguous.
DROGON_TEST(AcceptRepeatedIdenticalContentLength)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    expectStatus(TEST_CTX,
                 client->getLoop(),
                 "PUT /api/v1/apitest/static HTTP/1.1\r\n"
                 "Host: localhost\r\n"
                 "Content-Length: 1\r\n"
                 "Content-Length: 1\r\n"
                 "Connection: close\r\n\r\n"
                 "x",
                 "HTTP/1.1 200");
}
