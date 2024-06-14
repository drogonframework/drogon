#include <drogon/HttpClient.h>
#include <drogon/drogon_test.h>
#include <string>
#include <iostream>
#include <fstream>

using namespace drogon;

DROGON_TEST(RequestStreamTest)
{
    auto client = HttpClient::newHttpClient("127.0.0.1", 8848);
    HttpRequestPtr req;

    req = HttpRequest::newHttpRequest();
    req->setPath("/stream_status");
    {
        auto [res, resp] = client->sendRequest(req);
        REQUIRE(res == ReqResult::Ok);
        REQUIRE(resp->statusCode() == k200OK);
        if (resp->body() != "enabled")
        {
            LOG_INFO << "Server does not enable request stream.";
            return;
        }
    }

    UploadFile file1("./中文.txt");
    std::ifstream file(file1.path());
    std::stringstream content;
    REQUIRE(file.is_open());
    content << file.rdbuf();

    LOG_INFO << "Test stream_upload";
    req = HttpRequest::newFileUploadRequest({file1});
    req->setPath("/stream_upload_echo");
    req->setMethod(Post);
    client->sendRequest(req,
                        [TEST_CTX,
                         content = content.str()](ReqResult r,
                                                  const HttpResponsePtr &resp) {
                            REQUIRE(r == ReqResult::Ok);
                            REQUIRE(resp->statusCode() == k200OK);
                            REQUIRE(resp->body() == content);
                        });
}
