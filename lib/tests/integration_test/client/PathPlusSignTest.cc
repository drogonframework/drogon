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
    auto client = std::make_shared<trantor::TcpClient>(
        loop, trantor::InetAddress{"127.0.0.1", 8848}, "path-plus-sign-test");
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

// The target goes on the wire verbatim, so the server sees it as written.
std::string get(trantor::EventLoop *loop, const std::string &target)
{
    return sendRawRequest(loop,
                          "GET " + target +
                              " HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Connection: close\r\n\r\n");
}

bool contains(const std::string &response, std::string_view expected)
{
    return response.find(expected) != std::string::npos;
}
}  // namespace

// A '+' in a path is a literal character (RFC 3986, section 3.3).
DROGON_TEST(PathKeepsPlusSign)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    auto loop = client->getLoop();

    auto response = get(loop, "/api/v1/path_echo/a+b.jpg");
    CHECK(contains(response, "path=/api/v1/path_echo/a+b.jpg\n"));
    CHECK(contains(response, "original=/api/v1/path_echo/a+b.jpg\n"));
    CHECK(contains(response, "name=a+b.jpg\n"));

    response = get(loop, "/api/v1/path_echo/a%2Bb+c%20d");
    CHECK(contains(response, "path=/api/v1/path_echo/a+b+c d\n"));
    CHECK(contains(response, "original=/api/v1/path_echo/a%2Bb+c%20d\n"));
    CHECK(contains(response, "name=a+b+c d\n"));
}

// Query strings are form-urlencoded: there a '+' still stands for a space.
DROGON_TEST(QueryStillDecodesPlusSignAsSpace)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);

    const auto response =
        get(client->getLoop(), "/api/v1/path_echo/a+b?q=c+d%2Be%20f");
    CHECK(contains(response, "path=/api/v1/path_echo/a+b\n"));
    CHECK(contains(response, "q=c d+e f\n"));
}

DROGON_TEST(StaticFileWithPlusSignInName)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    auto loop = client->getLoop();

    CHECK(get(loop, "/a-directory/a+b.html").rfind("HTTP/1.1 200", 0) == 0);
    CHECK(get(loop, "/a-directory/a%2Bb.html").rfind("HTTP/1.1 200", 0) == 0);
    CHECK(get(loop, "/a-directory/a%20b.html").rfind("HTTP/1.1 404", 0) == 0);
}

// HttpClient escapes a '+' and writes a space as %20, so that a path set with
// setPath() reaches the server unchanged.
DROGON_TEST(ClientPathRoundTrip)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/api/v1/path_echo/a+b c");

    const auto [result, resp] = client->sendRequest(req, 10);
    REQUIRE(result == ReqResult::Ok);
    const std::string body{resp->getBody()};
    CHECK(contains(body, "path=/api/v1/path_echo/a+b c\n"));
    CHECK(contains(body, "original=/api/v1/path_echo/a%2Bb%20c\n"));
}
