#include <drogon/HttpClient.h>
#include <drogon/drogon_test.h>
#include <trantor/net/TcpClient.h>
#include <chrono>
#include <string>
#include <iostream>
#include <fstream>

using namespace drogon;

template <typename T>
void checkStreamRequest(T &&TEST_CTX,
                        trantor::EventLoop *loop,
                        const trantor::InetAddress &addr,
                        const std::vector<std::string_view> &dataToSend,
                        std::string_view expectedResp)
{
    auto tcpClient = std::make_shared<trantor::TcpClient>(loop, addr, "test");
    std::promise<void> promise;

    auto respString = std::make_shared<std::string>();
    tcpClient->setMessageCallback(
        [respString](const trantor::TcpConnectionPtr &conn,
                     trantor::MsgBuffer *buf) {
            respString->append(buf->read(buf->readableBytes()));
        });
    tcpClient->setConnectionCallback(
        [TEST_CTX, &promise, respString, dataToSend, expectedResp](
            const trantor::TcpConnectionPtr &conn) {
            if (conn->disconnected())
            {
                LOG_INFO << "Disconnected from server";
                CHECK(respString->substr(0, expectedResp.size()) ==
                      expectedResp);
                promise.set_value();
                return;
            }
            LOG_INFO << "Connected to server";
            CHECK(conn->connected());
            for (auto &data : dataToSend)
            {
                conn->send(data.data(), data.size());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            conn->shutdown();
        });
    tcpClient->connect();
    promise.get_future().wait();
}

DROGON_TEST(RequestStreamTest)
{
    const std::string ip = "127.0.0.1";
    const uint16_t port = 8848;
    auto client = HttpClient::newHttpClient(ip, port);
    HttpRequestPtr req;

    bool enabled = false;

    req = HttpRequest::newHttpRequest();
    req->setPath("/stream_status");
    {
        auto [res, resp] = client->sendRequest(req);
        REQUIRE(res == ReqResult::Ok);
        REQUIRE(resp->statusCode() == k200OK);
        if (resp->body() == "enabled")
        {
            enabled = true;
        }
        else
        {
            LOG_INFO << "Server does not enable request stream.";
        }
    }

    req = HttpRequest::newHttpRequest();
    req->setPath("/stream_chunk");
    req->setMethod(Post);
    req->setBody("1234567890");
    client->sendRequest(req,
                        [TEST_CTX, enabled](ReqResult r,
                                            const HttpResponsePtr &resp) {
                            REQUIRE(r == ReqResult::Ok);
                            if (enabled)
                            {
                                CHECK(resp->statusCode() == k200OK);
                                CHECK(resp->body() == "1234567890");
                            }
                            else
                            {
                                CHECK(resp->statusCode() == k400BadRequest);
                                CHECK(resp->body() == "no stream");
                            }
                        });

    if (!enabled)
    {
        return;
    }

    LOG_INFO << "Test request stream";

    std::string filePath = "./中文.txt";
    std::ifstream file(filePath);
    std::stringstream content;
    REQUIRE(file.is_open());
    content << file.rdbuf();

    req = HttpRequest::newFileUploadRequest({UploadFile{filePath}});
    req->setPath("/stream_upload_echo");
    req->setMethod(Post);
    client->sendRequest(req,
                        [TEST_CTX,
                         content = content.str()](ReqResult r,
                                                  const HttpResponsePtr &resp) {
                            CHECK(r == ReqResult::Ok);
                            CHECK(resp->statusCode() == k200OK);
                            CHECK(resp->body() == content);
                        });

    checkStreamRequest(TEST_CTX,
                       client->getLoop(),
                       trantor::InetAddress{ip, port},
                       // Good request
                       {"POST /stream_chunk HTTP/1.1\r\n"
                        "Transfer-Encoding: chunked\r\n\r\n",
                        "1\r\nz\r\n",
                        "2\r\nzz\r\n0\r\n\r\n"},
                       // Good response
                       "HTTP/1.1 200 OK\r\n");

    checkStreamRequest(TEST_CTX,
                       client->getLoop(),
                       trantor::InetAddress{ip, port},
                       // Bad request
                       {"POST /stream_chunk HTTP/1.1\r\n"
                        "Transfer-Encoding: chunked\r\n\r\n",
                        "1\r\nz\r\n",
                        "1\r\nzz\r\n",
                        "0\r\n\r\n"},
                       // Bad response
                       "HTTP/1.1 400 Bad Request\r\n");
}
