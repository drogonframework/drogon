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

#include <drogon/drogon.h>
#include <trantor/net/EventLoopThread.h>
#include <trantor/net/TcpClient.h>

#include <mutex>
#include <future>
#ifndef _WIN32
#include <unistd.h>
#endif

#define RESET "\033[0m"
#define RED "\033[31m"   /* Red */
#define GREEN "\033[32m" /* Green */

#define JPG_LEN 44618
#define INDEX_LEN 10606

using namespace drogon;

Cookie sessionID;

void outputGood(const HttpRequestPtr &req, bool isHttps)
{
    static int i = 0;
    // auto start = req->creationDate();
    // auto end = trantor::Date::now();
    // int ms = end.microSecondsSinceEpoch() - start.microSecondsSinceEpoch();
    // ms = ms / 1000;
    // char str[32];
    // sprintf(str, "%6dms", ms);
    static std::mutex mtx;
    {
        std::lock_guard<std::mutex> lock(mtx);
        ++i;
        std::cout << i << GREEN << '\t' << "Good" << '\t' << RED
                  << req->methodString() << " " << req->path();
        if (isHttps)
            std::cout << "\t(https)";
        std::cout << RESET << std::endl;
    }
}
void doTest(const HttpClientPtr &client,
            std::promise<int> &pro,
            bool isHttps = false)
{
    /// Get cookie
    if (!sessionID)
    {
        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/");
        std::promise<int> waitCookie;
        auto f = waitCookie.get_future();
        client->sendRequest(req,
                            [client, &waitCookie](ReqResult result,
                                                  const HttpResponsePtr &resp) {
                                if (result == ReqResult::Ok)
                                {
                                    auto &id = resp->getCookie("JSESSIONID");
                                    if (!id)
                                    {
                                        LOG_ERROR << "Error!";
                                        exit(1);
                                    }
                                    sessionID = id;
                                    client->addCookie(id);
                                    waitCookie.set_value(1);
                                }
                                else
                                {
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            });
        f.get();
    }
    /// Test session
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/slow");
    client->sendRequest(
        req,
        [req, isHttps, client](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                outputGood(req, isHttps);
                auto req1 = HttpRequest::newHttpRequest();
                req1->setMethod(drogon::Get);
                req1->setPath("/slow");
                client->sendRequest(
                    req1,
                    [req1, isHttps](ReqResult result,
                                    const HttpResponsePtr &resp1) {
                        if (result == ReqResult::Ok)
                        {
                            auto &json = resp1->jsonObject();
                            if (json && json->get("message", std::string(""))
                                                .asString() ==
                                            "Access interval should be "
                                            "at least 10 "
                                            "seconds")
                            {
                                outputGood(req1, isHttps);
                            }
                            else
                            {
                                LOG_DEBUG << resp1->body();
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        }
                        else
                        {
                            LOG_ERROR << "Error!";
                            exit(1);
                        }
                    });
            }
            else
            {
                LOG_ERROR << "Error!";
                exit(1);
            }
        });
    /// Test gzip
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->addHeader("accept-encoding", "gzip");
    req->setPath("/api/v1/apitest/get/111");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->getBody().length() == 4994)
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody().length();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
/// Test brotli
#ifdef USE_BROTLI
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->addHeader("accept-encoding", "br");
    req->setPath("/api/v1/apitest/get/111");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->getBody().length() == 4994)
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody().length();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
#endif
    /// Post json
    Json::Value json;
    json["request"] = "json";
    req = HttpRequest::newCustomHttpRequest(json);
    req->setMethod(drogon::Post);
    req->setPath("/api/v1/apitest/json");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                std::shared_ptr<Json::Value> ret = *resp;
                                if (ret && (*ret)["result"].asString() == "ok")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
    // Post json again
    req = HttpRequest::newHttpJsonRequest(json);
    assert(req->jsonObject());
    req->setMethod(drogon::Post);
    req->setPath("/api/v1/apitest/json");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                std::shared_ptr<Json::Value> ret = *resp;
                                if (ret && (*ret)["result"].asString() == "ok")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
    // Post json again
    req = HttpRequest::newHttpJsonRequest(json);
    req->setMethod(drogon::Post);
    req->setPath("/api/v1/apitest/json");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                std::shared_ptr<Json::Value> ret = *resp;
                                if (ret && (*ret)["result"].asString() == "ok")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    /// 1 Get /
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                // LOG_DEBUG << resp->getBody();
                                if (resp->getBody() == "<p>Hello, world!</p>")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
    /// 3. Post to /tpost to test Http Method constraint
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/tpost");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                // LOG_DEBUG << resp->getBody();
                                if (resp->statusCode() == k405MethodNotAllowed)
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/tpost");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->getBody() == "<p>Hello, world!</p>")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    /// 4. Http OPTIONS Method
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Options);
    req->setPath("/tpost");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                // LOG_DEBUG << resp->getBody();
                                auto allow = resp->getHeader("allow");
                                if (resp->statusCode() == k200OK &&
                                    allow.find("POST") != std::string::npos)
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Options);
    req->setPath("/api/v1/apitest");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                // LOG_DEBUG << resp->getBody();
                                auto allow = resp->getHeader("allow");
                                if (resp->statusCode() == k200OK &&
                                    allow == "OPTIONS,GET,HEAD,POST")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Options);
    req->setPath("/slow");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->statusCode() == k403Forbidden)
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Options);
    req->setPath("/*");
    client->sendRequest(
        req, [req, isHttps](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                // LOG_DEBUG << resp->getBody();
                auto allow = resp->getHeader("allow");
                if (resp->statusCode() == k200OK &&
                    allow == "GET,HEAD,POST,PUT,DELETE,OPTIONS,PATCH")
                {
                    outputGood(req, isHttps);
                }
                else
                {
                    LOG_ERROR << "Error!";
                    exit(1);
                }
            }
            else
            {
                LOG_ERROR << "Error!";
                exit(1);
            }
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Options);
    req->setPath("/api/v1/apitest/static");
    client->sendRequest(
        req, [req, isHttps](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                // LOG_DEBUG << resp->getBody();
                auto allow = resp->getHeader("allow");
                if (resp->statusCode() == k200OK && allow == "OPTIONS,GET,HEAD")
                {
                    outputGood(req, isHttps);
                }
                else
                {
                    LOG_ERROR << "Error!";
                    exit(1);
                }
            }
            else
            {
                LOG_ERROR << "Error!";
                exit(1);
            }
        });

    /// 4. Test HttpController
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/v1/apitest");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->getBody() == "ROOT Post!!!")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->getBody() == "ROOT Get!!!")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest/get/abc/123");
    client->sendRequest(
        req, [req, isHttps](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                if (resp->getBody().find("<td>p1</td>\n        <td>123</td>") !=
                        std::string::npos &&
                    resp->getBody().find("<td>p2</td>\n        <td>abc</td>") !=
                        std::string::npos)
                {
                    outputGood(req, isHttps);
                }
                else
                {
                    LOG_DEBUG << resp->getBody();
                    LOG_ERROR << "Error!";
                    exit(1);
                }
            }
            else
            {
                LOG_ERROR << "Error!";
                exit(1);
            }
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest/3.14/List");
    req->setParameter("P2", "1234");
    client->sendRequest(
        req, [req, isHttps](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                if (resp->getBody().find(
                        "<td>p1</td>\n        <td>3.140000</td>") !=
                        std::string::npos &&
                    resp->getBody().find(
                        "<td>p2</td>\n        <td>1234</td>") !=
                        std::string::npos)
                {
                    outputGood(req, isHttps);
                }
                else
                {
                    LOG_DEBUG << resp->getBody();
                    LOG_ERROR << "Error!";
                    exit(1);
                }
            }
            else
            {
                LOG_ERROR << "Error!";
                exit(1);
            }
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/reg/123/rest/of/the/path");
    client->sendRequest(
        req, [req, isHttps](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok && resp->getJsonObject())
            {
                auto &json = resp->getJsonObject();
                if (json->isMember("p1") && json->get("p1", 0).asInt() == 123 &&
                    json->isMember("p2") &&
                    json->get("p2", "").asString() == "rest/of/the/path")
                {
                    outputGood(req, isHttps);
                }
                else
                {
                    LOG_DEBUG << resp->getBody();
                    LOG_ERROR << "Error!";
                    exit(1);
                }
            }
            else
            {
                LOG_ERROR << "Error!";
                exit(1);
            }
        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest/static");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->getBody() == "staticApi,hello!!")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    // auto loop = app().loop();

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/v1/apitest/static");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->getBody() == "staticApi,hello!!")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest/get/111");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->getBody().length() == 4994)
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    // LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    /// Test static function
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/handle11/11/2 2/?p3=3 x");
    req->setParameter("p4", "44");
    client->sendRequest(
        req, [req, isHttps](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                if (resp->getBody().find(
                        "<td>int p1</td>\n        <td>11</td>") !=
                        std::string::npos &&
                    resp->getBody().find(
                        "<td>int p4</td>\n        <td>44</td>") !=
                        std::string::npos &&
                    resp->getBody().find(
                        "<td>string p2</td>\n        <td>2 2</td>") !=
                        std::string::npos &&
                    resp->getBody().find(
                        "<td>string p3</td>\n        <td>3 x</td>") !=
                        std::string::npos)
                {
                    outputGood(req, isHttps);
                }
                else
                {
                    LOG_DEBUG << resp->getBody();
                    LOG_ERROR << "Error!";
                    exit(1);
                }
            }
            else
            {
                LOG_ERROR << "Error!";
                exit(1);
            }
        });
    /// Test Incomplete URL
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/handle11/11/2 2/");
    client->sendRequest(
        req, [req, isHttps](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                if (resp->getBody().find(
                        "<td>int p1</td>\n        <td>11</td>") !=
                        std::string::npos &&
                    resp->getBody().find(
                        "<td>int p4</td>\n        <td>0</td>") !=
                        std::string::npos &&
                    resp->getBody().find(
                        "<td>string p2</td>\n        <td>2 2</td>") !=
                        std::string::npos &&
                    resp->getBody().find(
                        "<td>string p3</td>\n        <td></td>") !=
                        std::string::npos)
                {
                    outputGood(req, isHttps);
                }
                else
                {
                    LOG_DEBUG << resp->getBody();
                    LOG_ERROR << "Error!";
                    exit(1);
                }
            }
            else
            {
                LOG_ERROR << "Error!";
                exit(1);
            }
        });
    /// Test lambda
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/handle2/111/222");
    client->sendRequest(
        req, [req, isHttps](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                if (resp->getBody().find("<td>a</td>\n        <td>111</td>") !=
                        std::string::npos &&
                    resp->getBody().find(
                        "<td>b</td>\n        <td>222.000000</td>") !=
                        std::string::npos)
                {
                    outputGood(req, isHttps);
                }
                else
                {
                    LOG_DEBUG << resp->getBody();
                    LOG_ERROR << "Error!";
                    exit(1);
                }
            }
            else
            {
                LOG_ERROR << "Error!";
                exit(1);
            }
        });

    /// Test std::bind and std::function
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/handle4/444/333/111");
    client->sendRequest(
        req, [req, isHttps](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                if (resp->getBody().find(
                        "<td>int p1</td>\n        <td>111</td>") !=
                        std::string::npos &&
                    resp->getBody().find(
                        "<td>int p4</td>\n        <td>444</td>") !=
                        std::string::npos &&
                    resp->getBody().find(
                        "<td>string p3</td>\n        <td>333</td>") !=
                        std::string::npos &&
                    resp->getBody().find(
                        "<td>string p2</td>\n        <td></td>") !=
                        std::string::npos)
                {
                    outputGood(req, isHttps);
                }
                else
                {
                    LOG_DEBUG << resp->getBody();
                    LOG_ERROR << "Error!";
                    exit(1);
                }
            }
            else
            {
                LOG_ERROR << "Error!";
                exit(1);
            }
        });
    /// Test gzip_static
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/index.html");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->getBody().length() == INDEX_LEN)
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody().length();
                                    LOG_ERROR << "Error!";
                                    LOG_ERROR << resp->getBody();
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/index.html");
    req->addHeader("accept-encoding", "gzip");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->getBody().length() == INDEX_LEN)
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody().length();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
    /// Test file download
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/drogon.jpg");
    client->sendRequest(
        req,
        [req, client, isHttps, &pro](ReqResult result,
                                     const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                if (resp->getBody().length() == JPG_LEN)
                {
                    outputGood(req, isHttps);
                    auto &lastModified = resp->getHeader("last-modified");
                    // LOG_DEBUG << lastModified;
                    // Test 'Not Modified'
                    auto req = HttpRequest::newHttpRequest();
                    req->setMethod(drogon::Get);
                    req->setPath("/drogon.jpg");
                    req->addHeader("If-Modified-Since", lastModified);
                    client->sendRequest(
                        req,
                        [req, isHttps, &pro](ReqResult result,
                                             const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->statusCode() == k304NotModified)
                                {
                                    outputGood(req, isHttps);
                                    pro.set_value(1);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody().length();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
                }
                else
                {
                    LOG_DEBUG << resp->getBody().length();
                    LOG_ERROR << "Error!";
                    exit(1);
                }
            }
            else
            {
                LOG_ERROR << "Error!";
                exit(1);
            }
        });

    /// Test file download, It is forbidden to download files from the
    /// parent folder
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/../../drogon.jpg");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->statusCode() == k403Forbidden)
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody().length();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
    /// Test controllers created and initialized by users
    req = HttpRequest::newHttpRequest();
    req->setPath("/customctrl/antao");
    req->addHeader("custom_header", "yes");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                auto ret = resp->getBody();
                                if (ret == "<P>Hi, antao</P>")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
    /// Test controllers created and initialized by users
    req = HttpRequest::newHttpRequest();
    req->setPath("/absolute/123");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->statusCode() == k200OK)
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
    /// Test synchronous advice
    req = HttpRequest::newHttpRequest();
    req->setPath("/plaintext");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->getBody() == "Hello, World!")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
    /// Test form post
    req = HttpRequest::newHttpFormPostRequest();
    req->setPath("/api/v1/apitest/form");
    req->setParameter("k1", "1");
    req->setParameter("k2", "安");
    req->setParameter("k3", "test@example.com");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                auto ret = resp->getJsonObject();
                                if (ret && (*ret)["result"].asString() == "ok")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    /// Test attributes
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest/attrs");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                auto ret = resp->getJsonObject();
                                if (ret && (*ret)["result"].asString() == "ok")
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });

    /// Test attachment download
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/attachment/download");
    client->sendRequest(req,
                        [req, isHttps](ReqResult result,
                                       const HttpResponsePtr &resp) {
                            if (result == ReqResult::Ok)
                            {
                                if (resp->getBody().length() == JPG_LEN)
                                {
                                    outputGood(req, isHttps);
                                }
                                else
                                {
                                    LOG_DEBUG << resp->getBody().length();
                                    LOG_ERROR << "Error!";
                                    exit(1);
                                }
                            }
                            else
                            {
                                LOG_ERROR << "Error!";
                                exit(1);
                            }
                        });
    // return;
    // Test file upload
    UploadFile file1("./drogon.jpg");
    UploadFile file2("./drogon.jpg", "drogon1.jpg");
    UploadFile file3("./config.example.json", "config.json", "file3");
    req = HttpRequest::newFileUploadRequest({file1, file2, file3});
    req->setPath("/api/attachment/upload");
    req->setParameter("P1", "upload");
    req->setParameter("P2", "test");
    client->sendRequest(
        req, [req, isHttps](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                auto json = resp->getJsonObject();
                if (json && (*json)["result"].asString() == "ok" &&
                    (*json)["P1"] == "upload" && (*json)["P2"] == "test")
                {
                    outputGood(req, isHttps);
                    // std::cout << (*json) << std::endl;
                }
                else
                {
                    LOG_DEBUG << resp->getBody().length();
                    LOG_DEBUG << resp->getBody();
                    LOG_ERROR << "Error!";
                    exit(1);
                }
            }
            else
            {
                LOG_ERROR << "Error!";
                exit(1);
            }
        });
}
int main(int argc, char *argv[])
{
    trantor::EventLoopThread loop[2];
    trantor::Logger::setLogLevel(trantor::Logger::LogLevel::kDebug);
    bool ever = false;
    if (argc > 1 && std::string(argv[1]) == "-f")
        ever = true;
    loop[0].run();
    loop[1].run();

    do
    {
        std::promise<int> pro1;
        auto client = HttpClient::newHttpClient("http://127.0.0.1:8848",
                                                loop[0].getLoop());
        client->setPipeliningDepth(10);
        if (sessionID)
            client->addCookie(sessionID);
        doTest(client, pro1);
        if (app().supportSSL())
        {
            std::promise<int> pro2;
            auto sslClient = HttpClient::newHttpClient("127.0.0.1",
                                                       8849,
                                                       true,
                                                       loop[1].getLoop());
            if (sessionID)
                sslClient->addCookie(sessionID);
            doTest(sslClient, pro2, true);
            auto f2 = pro2.get_future();
            f2.get();
        }
        auto f1 = pro1.get_future();
        f1.get();
    } while (ever);
    // getchar();
    loop[0].getLoop()->quit();
    loop[1].getLoop()->quit();
    return 0;
}
