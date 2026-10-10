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
                                             "static-file-traversal-test");
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

// The target goes on the wire verbatim, so encodings reach the server as is.
void expectStatus(std::shared_ptr<test::Case> TEST_CTX,
                  trantor::EventLoop *loop,
                  const std::string &target,
                  std::string_view status)
{
    const auto response = sendRawRequest(loop,
                                         "GET " + target +
                                             " HTTP/1.1\r\n"
                                             "Host: localhost\r\n"
                                             "Connection: close\r\n\r\n");
    if (response.rfind(status, 0) != 0)
    {
        FAIL(target,
             " answered '",
             response.substr(0, response.find("\r\n")),
             "' instead of ",
             status);
        return;
    }
    SUCCESS();
}
}  // namespace

// Any encoding of a path that folds above the document root is refused.
DROGON_TEST(StaticFileRejectsDocumentRootEscape)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    expectStatus(TEST_CTX,
                 client->getLoop(),
                 "/a-directory/..%2findex.html",
                 "HTTP/1.1 200");
    for (const auto &target : {"/../main.cc",
                               "/..%2fmain.cc",
                               "/%2e%2e/%2e%2e/main.cc",
                               "/a-directory/..%2f..%2fmain.cc",
                               "/a-directory/%2e%2e/.%2e/main.cc"})
    {
        expectStatus(TEST_CTX, client->getLoop(), target, "HTTP/1.1 403");
    }
}

// A backslash is a separator only on Windows; main.cc must never be served.
DROGON_TEST(StaticFileBackslashFollowsPlatform)
{
#ifdef _WIN32
    const std::string_view status = "HTTP/1.1 403";
#else
    const std::string_view status = "HTTP/1.1 404";
#endif
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    for (const auto &target : {"/..%5c..%5cmain.cc",
                               "/%2e%2e%5cmain.cc",
                               "/a-directory/..%5c..%5cmain.cc"})
    {
        expectStatus(TEST_CTX, client->getLoop(), target, status);
    }
}

// The alias directory, not the document root, bounds a location.
DROGON_TEST(StaticFileLocationStaysInsideAlias)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    expectStatus(TEST_CTX,
                 client->getLoop(),
                 "/a-directory-alias/page.html",
                 "HTTP/1.1 200");
    for (const auto &target : {"/a-directory-alias/../main.cc",
                               "/a-directory-alias/..%2fmain.cc",
                               "/a-directory-alias/%2e%2e/index.html"})
    {
        expectStatus(TEST_CTX, client->getLoop(), target, "HTTP/1.1 403");
    }
}

DROGON_TEST(StaticFileNonRecursiveLocation)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    expectStatus(TEST_CTX, client->getLoop(), "/flat/main.cc", "HTTP/1.1 200");
    expectStatus(TEST_CTX,
                 client->getLoop(),
                 "/flat/a-directory/page.html",
                 "HTTP/1.1 403");
}
