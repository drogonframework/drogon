/**
 *
 *  @file test.cc
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

// Make a http client to test the example server app;

#define DROGON_TEST_MAIN
#include <drogon/drogon.h>
#include <trantor/net/EventLoopThread.h>
#include <trantor/net/TcpClient.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/drogon_test.h>

#include <mutex>
#include <algorithm>
#include <atomic>
#include <chrono>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <sys/stat.h>

#define JPG_LEN 44618UL
size_t indexLen;
size_t indexImplicitLen;

using namespace drogon;

Cookie sessionID;

void doTest(const HttpClientPtr &client, std::shared_ptr<test::Case> TEST_CTX)
{
    /// Get cookie
    if (!sessionID)
    {
        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/");
        std::promise<int> waitCookie;
        bool haveCert = false;
        auto f = waitCookie.get_future();
        client->sendRequest(req,
                            [client, &waitCookie, &haveCert, TEST_CTX](
                                ReqResult result, const HttpResponsePtr &resp) {
                                REQUIRE(result == ReqResult::Ok);

                                auto &id = resp->getCookie("JSESSIONID");
                                REQUIRE(id);

                                haveCert = resp->peerCertificate() != nullptr;

                                sessionID = id;
                                client->addCookie(id);
                                waitCookie.set_value(1);
                            });
        f.get();
        CHECK(haveCert == client->secure());
    }
    else
        client->addCookie(sessionID);

    /// Test begin advice
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/test_begin_advice");
    client->sendRequest(req,
                        [req, client, TEST_CTX](ReqResult result,
                                                const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody() == "DrogonReady");
                        });

    /// Test session
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/slow");
    client->sendRequest(
        req,
        [req, client, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);

            auto req1 = HttpRequest::newHttpRequest();
            req1->setMethod(drogon::Get);
            req1->setPath("/slow");
            client->sendRequest(
                req1,
                [req1, TEST_CTX](ReqResult result,
                                 const HttpResponsePtr &resp1) {
                    REQUIRE(result == ReqResult::Ok);

                    auto &json = resp1->jsonObject();
                    REQUIRE(json != nullptr);
                    REQUIRE(json->get("message", std::string("")).asString() ==
                            "Access interval should be "
                            "at least 10 "
                            "seconds");
                });
        });
    /// Test gzip
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->addHeader("accept-encoding", "gzip");
    req->setPath("/api/v1/apitest/get/111");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody().length() == 4994UL);
                        });
/// Test brotli
#ifdef USE_BROTLI
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->addHeader("accept-encoding", "br");
    req->setPath("/api/v1/apitest/get/111");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody().length() == 4994UL);
                        });
#endif
    /// Post json
    Json::Value json;
    json["request"] = "json";
    req = HttpRequest::newCustomHttpRequest(json);
    req->setMethod(drogon::Post);
    req->setPath("/api/v1/apitest/json");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);

                            std::shared_ptr<Json::Value> ret = *resp;
                            REQUIRE(ret != nullptr);
                            CHECK((*ret)["result"].asString() == "ok");
                        });
    // Post json again
    req = HttpRequest::newHttpJsonRequest(json);
    assert(req->jsonObject());
    req->setMethod(drogon::Post);
    req->setPath("/api/v1/apitest/json");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);

                            std::shared_ptr<Json::Value> ret = *resp;
                            REQUIRE(ret != nullptr);
                            CHECK((*ret)["result"].asString() == "ok");
                        });

    // Post json again
    req = HttpRequest::newHttpJsonRequest(json);
    req->setMethod(drogon::Post);
    req->setPath("/api/v1/apitest/json");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);

                            std::shared_ptr<Json::Value> ret = *resp;
                            REQUIRE(ret != nullptr);
                            CHECK((*ret)["result"].asString() == "ok");
                        });

    // Test 404
    req = HttpRequest::newHttpJsonRequest(json);
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest/notFoundRouting");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k404NotFound);
                        });
    /// 1 Get /
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody() == "<p>Hello, world!</p>");
                            // LOG_DEBUG << resp->getBody();
                        });
    /// 3. Post to /tpost to test Http Method constraint
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/tpost");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getStatusCode() == k405MethodNotAllowed);
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/tpost");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody() == "<p>Hello, world!</p>");
                        });

    /// 4. Http OPTIONS Method
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Options);
    req->setPath("/tpost");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->statusCode() == k200OK);
                            auto allow = resp->getHeader("allow");
                            CHECK(allow.find("POST") != std::string::npos);
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Options);
    req->setPath("/api/v1/apitest");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->statusCode() == k200OK);
                            auto allow = resp->getHeader("allow");
                            CHECK(allow == "OPTIONS,GET,HEAD,POST");
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Options);
    req->setPath("/slow");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->statusCode() == k403Forbidden);
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Options);
    req->setPath("/*");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->statusCode() == k200OK);
            auto allow = resp->getHeader("allow");
            CHECK(allow == "GET,HEAD,POST,PUT,DELETE,OPTIONS,PATCH");
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Options);
    req->setPath("/api/v1/apitest/static");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->statusCode() == k200OK);
                            auto allow = resp->getHeader("allow");
                            CHECK(allow == "OPTIONS,GET,HEAD");
                        });

    /// 4. Test HttpController
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/v1/apitest");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody() == "ROOT Post!!!");
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody() == "ROOT Get!!!");
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest/get/abc/123");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            auto body = resp->getBody();
            CHECK(body.find("<td>p1</td>\n        <td>123</td>") !=
                  std::string::npos);
            CHECK(body.find("<td>p2</td>\n        <td>abc</td>") !=
                  std::string::npos);
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest/3.14/List");
    req->setParameter("P2", "1234");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            auto body = resp->getBody();
            CHECK(body.find("<td>p1</td>\n        <td>3.140000</td>") !=
                  std::string::npos);
            CHECK(body.find("<td>p2</td>\n        <td>1234</td>") !=
                  std::string::npos);
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/reg/123/rest/of/the/path");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            REQUIRE(resp->getJsonObject() != nullptr);

            auto &json = resp->getJsonObject();
            CHECK(json->isMember("p1"));
            CHECK(json->get("p1", 0).asInt() == 123);
            CHECK(json->isMember("p2"));
            CHECK(json->get("p2", "").asString() == "rest/of/the/path");
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest/static");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody() == "staticApi,hello!!");
                        });

    // auto loop = app().loop();

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/v1/apitest/static");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody() == "staticApi,hello!!");
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest/get/111");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody().length() == 4994UL);
                        });

    /// Test method routing, see MethodTest.h
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/method/regex/drogon/test?test=drogon");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            auto jsonPtr = resp->getJsonObject();
                            CHECK(jsonPtr);
                            CHECK(jsonPtr->asString() == "POST");
                        });
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/method/regex/drogon/test");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            auto jsonPtr = resp->getJsonObject();
                            CHECK(jsonPtr);
                            CHECK(jsonPtr->asString() == "GET");
                        });
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/method/drogon/test?test=drogon");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            auto jsonPtr = resp->getJsonObject();
                            CHECK(jsonPtr);
                            CHECK(jsonPtr->asString() == "POST");
                        });
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/method/drogon/test");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            auto jsonPtr = resp->getJsonObject();
                            CHECK(jsonPtr);
                            CHECK(jsonPtr->asString() == "GET");
                        });
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/method/test?test=drogon");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            auto jsonPtr = resp->getJsonObject();
                            CHECK(jsonPtr);
                            CHECK(jsonPtr->asString() == "POST");
                        });
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/method/test");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            auto jsonPtr = resp->getJsonObject();
                            CHECK(jsonPtr);
                            CHECK(jsonPtr->asString() == "GET");
                        });
    /// Test static function
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/handle11/11/2 2/?p3=3 x");
    req->setParameter("p4", "44");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            auto body = resp->getBody();
            CHECK(body.find("<td>int p1</td>\n        <td>11</td>") !=
                  std::string::npos);
            CHECK(body.find("<td>int p4</td>\n        <td>44</td>") !=
                  std::string::npos);
            CHECK(body.find("<td>string p2</td>\n        <td>2 2</td>") !=
                  std::string::npos);
            CHECK(body.find("<td>string p3</td>\n        <td>3 x</td>") !=
                  std::string::npos);
        });
    /// Test Incomplete URL
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/handle11/11/2 2/");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            auto body = resp->getBody();
            CHECK(body.find("<td>int p1</td>\n        <td>11</td>") !=
                  std::string::npos);
            CHECK(body.find("<td>int p4</td>\n        <td>0</td>") !=
                  std::string::npos);
            CHECK(body.find("<td>string p2</td>\n        <td>2 2</td>") !=
                  std::string::npos);
            CHECK(body.find("<td>string p3</td>\n        <td></td>") !=
                  std::string::npos);
        });
    /// Test lambda
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/handle2/111/222");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            auto body = resp->getBody();
            CHECK(body.find("<td>a</td>\n        <td>111</td>") !=
                  std::string::npos);
            CHECK(body.find("<td>b</td>\n        <td>222.000000</td>") !=
                  std::string::npos);
        });

    /// Test std::bind and std::function
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/handle4/444/333/111");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            auto body = resp->getBody();
            CHECK(body.find("<td>int p1</td>\n        <td>111</td>") !=
                  std::string::npos);
            CHECK(body.find("<td>int p4</td>\n        <td>444</td>") !=
                  std::string::npos);
            CHECK(body.find("<td>string p2</td>\n        <td></td>") !=
                  std::string::npos);
            CHECK(body.find("<td>string p3</td>\n        <td>333</td>") !=
                  std::string::npos);
        });
    /// Test gzip_static
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/index.html");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getBody().length() == indexLen);
            CHECK(resp->contentType() == CT_TEXT_HTML);
            CHECK(resp->contentTypeString().find("text/html") == 0);
        });
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/index.html");
    req->addHeader("accept-encoding", "gzip");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->contentType() == CT_TEXT_HTML);
                            CHECK(resp->getBody().length() == indexLen);
                        });
    /// Test serving file with non-ASCII files
    req = HttpRequest::newHttpRequest();
    req->setPath("/中文.txt");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            // Only tests for serving a file, not the content
                            // since no good way to read it on Windows without
                            // using the wild-char API
                        });
    /// Test custom content types
    req = HttpRequest::newHttpRequest();
    req->setPath("/test.md");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->contentType() == CT_CUSTOM);
                            CHECK(resp->contentTypeString() == "text/markdown");
                        });
    /// Unknown files should be application/octet-stream
    req = HttpRequest::newHttpRequest();
    req->setPath("/main.cc");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            REQUIRE(resp->contentType() == CT_APPLICATION_OCTET_STREAM);
        });
    // Test 405
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/drogon.jpg");
    client->sendRequest(req,
                        [req, client, TEST_CTX](ReqResult result,
                                                const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() ==
                                  k405MethodNotAllowed);
                        });
    /// Test file download
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/drogon.jpg");
    client->sendRequest(
        req,
        [req, client, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            REQUIRE(resp->getBody().length() == JPG_LEN);
            auto &lastModified = resp->getHeader("last-modified");
            // LOG_DEBUG << lastModified;
            // Test 'Not Modified'
            auto req = HttpRequest::newHttpRequest();
            req->setMethod(drogon::Get);
            req->setPath("/drogon.jpg");
            req->addHeader("If-Modified-Since", lastModified);
            client->sendRequest(req,
                                [req, TEST_CTX](ReqResult result,
                                                const HttpResponsePtr &resp) {
                                    REQUIRE(result == ReqResult::Ok);
                                    REQUIRE(resp->statusCode() ==
                                            k304NotModified);
                                    // pro.set_value(1);
                                });
        });

    /// Test file download, It is forbidden to download files from the
    /// parent folder
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/../../drogon.jpg");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->statusCode() == k403Forbidden);
                        });
    /// Test controllers created and initialized by users
    req = HttpRequest::newHttpRequest();
    req->setPath("/customctrl/antao");
    req->addHeader("custom_header", "yes");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody() == "<P>Hi, antao</P>");
                        });
    /// Test controllers created and initialized by users
    req = HttpRequest::newHttpRequest();
    req->setPath("/absolute/123");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->statusCode() == k200OK);
                        });
    /// Test synchronous advice
    req = HttpRequest::newHttpRequest();
    req->setPath("/plaintext");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody() == "Hello, World!");
                        });
    /// Test form post
    req = HttpRequest::newHttpFormPostRequest();
    req->setPath("/api/v1/apitest/form");
    req->setParameter("k1", "1");
    req->setParameter("k2", "安");
    req->setParameter("k3", "test@example.com");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            auto ret = resp->getJsonObject();
                            REQUIRE(ret != nullptr);
                            CHECK((*ret)["result"].asString() == "ok");
                        });

    /// Test attributes
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest/attrs");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            auto ret = resp->getJsonObject();
                            REQUIRE(ret != nullptr);
                            CHECK((*ret)["result"].asString() == "ok");
                        });

    /// Test attachment download
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/attachment/download");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody().length() == JPG_LEN);
                        });
    // Test implicit pages
    auto body = std::make_shared<std::string>();
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/a-directory");
    client->sendRequest(req,
                        [req, TEST_CTX, body](ReqResult result,
                                              const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody().length() == indexImplicitLen);
                            *body = std::string(resp->getBody().data(),
                                                resp->getBody().length());
                        });
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/a-directory/page.html");
    client->sendRequest(req,
                        [req, TEST_CTX, body](ReqResult result,
                                              const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody().length() == indexImplicitLen);
                            CHECK(std::equal(body->begin(),
                                             body->end(),
                                             resp->getBody().begin()));
                        });
    // Test file upload
    UploadFile file1("./中文.txt");
    UploadFile file2("./drogon.jpg", "drogon1.jpg");
    UploadFile file3("./config.example.json", "config.json", "file3");
    req = HttpRequest::newFileUploadRequest({file1, file2, file3});
    req->setPath("/api/attachment/upload");
    req->setParameter("P1", "upload");
    req->setParameter("P2", "test");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            auto json = resp->getJsonObject();
                            REQUIRE(json != nullptr);
                            CHECK((*json)["result"].asString() == "ok");
                            CHECK((*json)["P1"] == "upload");
                            CHECK((*json)["P2"] == "test");
                        });

    // Test file upload, file type and extension interface.
    UploadFile image("./drogon.jpg");
    req = HttpRequest::newFileUploadRequest({image});
    req->setPath("/api/attachment/uploadImage");
    req->setParameter("P1", "upload");
    req->setParameter("P2", "test");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            auto json = resp->getJsonObject();
                            REQUIRE(json != nullptr);
                            CHECK((*json)["P1"] == "upload");
                            CHECK((*json)["P2"] == "test");
                        });

    // Test newFileResponse
    req = HttpRequest::newHttpRequest();
    req->setPath("/RangeTestController/");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getStatusCode() == k200OK);
            CHECK(resp->getBody().size() == 1'000'000);
            CHECK(resp->getHeader("Content-Length") == "1000000");
        });

    req = HttpRequest::newHttpRequest();
    req->setPath("/RangeTestController/999980/0");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k206PartialContent);
                            CHECK(resp->getBody() == "01234567890123456789");
                        });

    // Test > 200k
    req = HttpRequest::newHttpRequest();
    req->setPath("/RangeTestController/1/500000");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getBody().size() == 500'000);
            CHECK(resp->getHeader("Content-Length") == "500000");
            CHECK(resp->getHeader("Content-Range") == "bytes 1-500000/1000000");
        });

    req = HttpRequest::newHttpRequest();
    req->setPath("/RangeTestController/10/20");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            LOG_DEBUG << "result=" << (int)result;
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getStatusCode() == k206PartialContent);
            CHECK(resp->getBody() == "01234567890123456789");
            CHECK(resp->getHeader("Content-Length") == "20");
            CHECK(resp->getHeader("Content-Range") == "bytes 10-29/1000000");
        });

    // Test invalid range
    req = HttpRequest::newHttpRequest();
    req->setPath("/RangeTestController/0/2000000");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getHeader("Content-Range") == "bytes */1000000");
            CHECK(resp->getStatusCode() == k416RequestedRangeNotSatisfiable);
        });

    //
    // Test StaticFileRouter with range header
    //
    req = HttpRequest::newHttpRequest();
    req->setPath("/range-test.txt");
    req->setMethod(drogon::Head);
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getStatusCode() == k200OK);
            CHECK(resp->getHeader("content-length") == "1000000");
            CHECK(resp->getHeader("accept-range") == "bytes");
        });

    req = HttpRequest::newHttpRequest();
    req->setPath("/range-test.txt");
    req->addHeader("range", "bytes=0-19");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k206PartialContent);
                            CHECK(resp->getBody() == "01234567890123456789");
                        });

    req = HttpRequest::newHttpRequest();
    req->setPath("/range-test.txt");
    req->addHeader("range", "bytes=-20");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k206PartialContent);
                            CHECK(resp->getBody() == "01234567890123456789");
                        });

    req = HttpRequest::newHttpRequest();
    req->setPath("/range-test.txt");
    req->addHeader("range", "bytes=999980-");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k206PartialContent);
                            CHECK(resp->getBody() == "01234567890123456789");
                        });

    // Using .. to access a upper directory should be permitted as long as
    // it never leaves the document root
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/a-directory/../index.html");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody().length() == indexLen);
                        });

    // . (current directory) shall also be allowed
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/a-directory/./page.html");
    client->sendRequest(req,
                        [req, TEST_CTX, body](ReqResult result,
                                              const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getBody().length() == indexImplicitLen);
                            CHECK(std::equal(body->begin(),
                                             body->end(),
                                             resp->getBody().begin()));
                        });

    // Test exception handling
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/this_will_fail");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getStatusCode() == k500InternalServerError);
        });

    // The result of this API is cached for (almost) forever. And the endpoint
    // increments a internal counter on each invoke. This tests if the respond
    // is taken from the cache after the first invoke.
    // Try poking the cache test endpoint 3 times. They should all respond 0
    // since the first respond is cached by the server.
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/ApiTest/cacheTest");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k200OK);
                            CHECK(resp->body() == "0");
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/ApiTest/cacheTest");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k200OK);
                            CHECK(resp->body() == "0");
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/ApiTest/cacheTest");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k200OK);
                            CHECK(resp->body() == "0");
                        });

    // This API caches it's result on the third (counting from 1) calls. Thus
    // we expect to always see 2 upon the third call. And all previous calls
    // should be less than or equal to 2, as another test is also poking the API
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/ApiTest/cacheTest2");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getStatusCode() == k200OK);
            int n = 100;
            CHECK_NOTHROW(n = std::stoi(std::string(resp->body())));
            CHECK(n <= 2);
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/ApiTest/cacheTest2");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getStatusCode() == k200OK);
            int n = 100;
            CHECK_NOTHROW(n = std::stoi(std::string(resp->body())));
            CHECK(n <= 2);
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/ApiTest/cacheTest2");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k200OK);
                            CHECK(resp->body() == "2");
                        });

    // Same as cacheTest2. But the server has to handle this API through regex.
    // it is intentionally made that the final part of the path can't contain
    // a "z" character
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/cacheTestRegex/foobar");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getStatusCode() == k200OK);
            int n = 100;
            CHECK_NOTHROW(n = std::stoi(std::string(resp->body())));
            CHECK(n <= 2);
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/cacheTestRegex/deadbeef");
    client->sendRequest(
        req, [req, TEST_CTX](ReqResult result, const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getStatusCode() == k200OK);
            int n = 100;
            CHECK_NOTHROW(n = std::stoi(std::string(resp->body())));
            CHECK(n <= 2);
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/cacheTestRegex/leet");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k200OK);
                            CHECK(resp->body() == "2");
                        });
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/cacheTestRegex/zebra");
    client->sendRequest(req,
                        [req, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k404NotFound);
                        });

    // Post compressed data
    req = HttpRequest::newHttpRequest();
    std::string deadbeef = "deadbeef";
    req->setPath("/api/v1/ApiTest/echoBody");
    req->addHeader("Content-Encoding", "gzip");
    req->setMethod(drogon::Post);
    req->setBody(utils::gzipCompress(deadbeef.c_str(), deadbeef.size()));
    client->sendRequest(req,
                        [deadbeef, TEST_CTX](ReqResult result,
                                             const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k200OK);
                            CHECK(resp->body() == deadbeef);
                        });

    std::string largeString(128 * 1024, 'a');  // 128KB of 'a'
    req = HttpRequest::newHttpRequest();
    req->setPath("/api/v1/ApiTest/echoBody");
    req->addHeader("Content-Encoding", "gzip");
    req->setMethod(drogon::Post);
    req->setBody(utils::gzipCompress(largeString.c_str(), largeString.size()));
    client->sendRequest(req,
                        [largeString, TEST_CTX](ReqResult result,
                                                const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k200OK);
                            CHECK(resp->body() == largeString);
                        });

#ifdef USE_BROTLI
    // Post compressed data
    req = HttpRequest::newHttpRequest();
    req->setPath("/api/v1/ApiTest/echoBody");
    req->addHeader("Content-Encoding", "br");
    req->setMethod(drogon::Post);
    req->setBody(utils::brotliCompress(deadbeef.c_str(), deadbeef.size()));
    client->sendRequest(req,
                        [deadbeef, TEST_CTX](ReqResult result,
                                             const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k200OK);
                            CHECK(resp->body() == deadbeef);
                        });
    req = HttpRequest::newHttpRequest();
    req->setPath("/api/v1/ApiTest/echoBody");
    req->addHeader("Content-Encoding", "br");
    req->setMethod(drogon::Post);
    req->setBody(
        utils::brotliCompress(largeString.c_str(), largeString.size()));
    client->sendRequest(req,
                        [largeString, TEST_CTX](ReqResult result,
                                                const HttpResponsePtr &resp) {
                            REQUIRE(result == ReqResult::Ok);
                            CHECK(resp->getStatusCode() == k200OK);
                            CHECK(resp->body() == largeString);
                        });
#endif

    // Test middleware
    req = HttpRequest::newHttpRequest();
    req->setPath("/test-middleware");
    client->sendRequest(req,
                        [TEST_CTX, req](ReqResult r,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(r == ReqResult::Ok);
                            CHECK(resp->body() == "1234test4321");
                        });

    req = HttpRequest::newHttpRequest();
    req->setPath("/test-middleware-block");
    client->sendRequest(req,
                        [TEST_CTX, req](ReqResult r,
                                        const HttpResponsePtr &resp) {
                            REQUIRE(r == ReqResult::Ok);
                            CHECK(resp->body() == "12block21");
                        });

#if defined(__cpp_impl_coroutine)
    async_run([client, TEST_CTX]() -> Task<> {
        // Test coroutine requests
        try
        {
            auto req = HttpRequest::newHttpRequest();
            req->setPath("/api/v1/corotest/get");
            auto resp = co_await client->sendRequestCoro(req);
            CHECK(resp->getBody() == "DEADBEEF");
        }
        catch (const std::exception &e)
        {
            FAIL("Unexpected exception, what()" + std::string(e.what()));
        }

        // Test coroutine request with co_return
        try
        {
            auto req = HttpRequest::newHttpRequest();
            req->setPath("/api/v1/corotest/get2");
            auto resp = co_await client->sendRequestCoro(req);
            CHECK(resp->getBody() == "BADDBEEF");
        }
        catch (const std::exception &e)
        {
            FAIL("Unexpected exception, what(): " + std::string(e.what()));
        }

        // Test Coroutine exception
        try
        {
            auto req = HttpRequest::newHttpRequest();
            req->setPath("/api/v1/corotest/this_will_fail");
            auto resp = co_await client->sendRequestCoro(req);
            CHECK(resp->getStatusCode() != k200OK);
        }
        catch (const std::exception &e)
        {
            FAIL("Unexpected exception, what(): " + std::string(e.what()));
        }

        // Test Coroutine exception with co_return
        try
        {
            auto req = HttpRequest::newHttpRequest();
            req->setPath("/api/v1/corotest/this_will_fail2");
            auto resp = co_await client->sendRequestCoro(req);
            CHECK(resp->getStatusCode() != k200OK);
        }
        catch (const std::exception &e)
        {
            FAIL("Unexpected exception, what(): " + std::string(e.what()));
        }

        // Test coroutine filter
        try
        {
            auto req = HttpRequest::newHttpRequest();
            auto start = std::chrono::system_clock::now();
            req->setPath("/api/v1/corotest/delay?secs=2");
            auto resp = co_await client->sendRequestCoro(req);
            CHECK(resp->getStatusCode() == k200OK);
            auto end = std::chrono::system_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            CHECK(duration.count() >= 2000);
        }
        catch (const std::exception &e)
        {
            FAIL("Unexpected exception, what(): " + std::string(e.what()));
        }

        // Test coroutine handler with parameters
        try
        {
            auto req = HttpRequest::newHttpRequest();
            req->setPath("/api/v1/corotest/get_with_param/some_data");
            auto resp = co_await client->sendRequestCoro(req);
            CHECK(resp->getStatusCode() == k200OK);
            CHECK(resp->getBody() == "some_data");
        }
        catch (const std::exception &e)
        {
            FAIL("Unexpected exception, what(): " + std::string(e.what()));
        }
        try
        {
            auto req = HttpRequest::newHttpRequest();
            req->setPath("/api/v1/corotest/get_with_param2/some_data");
            auto resp = co_await client->sendRequestCoro(req);
            CHECK(resp->getStatusCode() == k200OK);
            CHECK(resp->getBody() == "some_data");
        }
        catch (const std::exception &e)
        {
            FAIL("Unexpected exception, what(): " + std::string(e.what()));
        }

        // Test coroutine middleware
        try
        {
            auto req = HttpRequest::newHttpRequest();
            req->setPath("/test-middleware-coro");
            auto resp = co_await client->sendRequestCoro(req);
            CHECK(resp->body() == "12corotestcoro21");
        }
        catch (const std::exception &e)
        {
            FAIL("Unexpected exception, what(): " + std::string(e.what()));
        }
    });
#endif
}

void loadFileLengths()
{
    struct stat filestat;
    if (stat("index.html", &filestat) < 0)
    {
        LOG_SYSERR << "Unable to retrieve index.html file sizes";
        exit(1);
    }
    indexLen = filestat.st_size;
    if (stat("a-directory/page.html", &filestat) < 0)
    {
        LOG_SYSERR << "Unable to retrieve a-directory/page.html file sizes";
        exit(1);
    }
    indexImplicitLen = filestat.st_size;
}

DROGON_TEST(HttpTest)
{
    auto client = HttpClient::newHttpClient("http://127.0.0.1:8848");
    client->setPipeliningDepth(10);
    REQUIRE(client->secure() == false);
    REQUIRE(client->port() == 8848);
    REQUIRE(client->host() == "127.0.0.1");
    REQUIRE(client->onDefaultPort() == false);

    doTest(client, TEST_CTX);
}

DROGON_TEST(HttpsTest)
{
    if (!app().supportSSL())
        return;

    auto client = HttpClient::newHttpClient("https://127.0.0.1:8849",
                                            app().getLoop(),
                                            false,
                                            false);
    client->setPipeliningDepth(10);
    REQUIRE(client->secure() == true);
    REQUIRE(client->port() == 8849);
    REQUIRE(client->host() == "127.0.0.1");
    REQUIRE(client->onDefaultPort() == false);

    doTest(client, TEST_CTX);
}

DROGON_TEST(HttpsTimeoutTest)
{
    if (!app().supportSSL())
        return;

    auto client = HttpClient::newHttpClient("https://127.0.0.1:8849",
                                            app().getLoop(),
                                            false,
                                            false);
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/api/v1/apitest/static");
    req->setMethod(drogon::Get);
    auto weakClient = std::weak_ptr<HttpClient>(client);
    auto weakReq = std::weak_ptr<HttpRequest>(req);
    client->sendRequest(
        req,
        [weakClient, weakReq, TEST_CTX](ReqResult result,
                                        const HttpResponsePtr &resp) {
            REQUIRE(result == ReqResult::Ok);
            CHECK(resp->getStatusCode() == k200OK);

            app().getLoop()->queueInLoop([weakClient, weakReq, TEST_CTX]() {
                CHECK(weakReq.expired());
                CHECK(weakClient.expired());
            });
        },
        60);
}

int main(int argc, char **argv)
{
    trantor::Logger::setLogLevel(trantor::Logger::LogLevel::kDebug);
    loadFileLengths();

    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();

    std::thread thr([&p1]() {
        app().getLoop()->queueInLoop([&p1]() { p1.set_value(); });
        app().run();
    });

    f1.get();
    int testStatus = test::run(argc, argv);
    app().getLoop()->queueInLoop([]() { app().quit(); });
    thr.join();
    return testStatus;
}
