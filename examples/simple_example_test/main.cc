/**
 *
 *  test.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

//Make a http client to test the example server app;

#include <drogon/drogon.h>
#include <mutex>
#define RESET "\033[0m"
#define RED "\033[31m"   /* Red */
#define GREEN "\033[32m" /* Green */

using namespace drogon;
void outputGood(const HttpRequestPtr &req)
{
    static int i = 0;
    static std::mutex mtx;
    {
        std::lock_guard<std::mutex> lock(mtx);
        i++;
        std::cout << i << GREEN << '\t' << "Good" << '\t' << RED << req->methodString()
                  << " " << req->path() << RESET << std::endl;
    }
}
int main()
{
    auto client = HttpClient::newHttpClient("http://127.0.0.1:8080");
    /// 1 Get /
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            //LOG_DEBUG << resp->getBody();
            if (resp->getBody() == "<p>Hello, world!</p>")
            {
                outputGood(req);
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
    /// 2. Get /slow to test simple controller, session and filter (cookie is not supported by HttpClient now)
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/slow");
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            //LOG_DEBUG << resp->getBody();
            if (resp->getBody() == "<p>Hello, world!</p>")
            {
                outputGood(req);
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
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            //LOG_DEBUG << resp->getBody();
            if (resp->statusCode() == HttpResponse::k405MethodNotAllowed)
            {
                outputGood(req);
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
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            if (resp->getBody() == "<p>Hello, world!</p>")
            {
                outputGood(req);
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
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            if (resp->getBody() == "ROOT Post!!!")
            {
                outputGood(req);
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
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            if (resp->getBody() == "ROOT Get!!!")
            {
                outputGood(req);
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
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            if (resp->getBody().find("<td>p1</td>\n        <td>123</td>") != std::string::npos &&
                resp->getBody().find("<td>p2</td>\n        <td>abc</td>") != std::string::npos)
            {
                outputGood(req);
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
    req->setPath("/api/v1/apitest/3.14/List?P2=1234");
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            if (resp->getBody().find("<td>p1</td>\n        <td>3.140000</td>") != std::string::npos &&
                resp->getBody().find("<td>p2</td>\n        <td>1234</td>") != std::string::npos)
            {
                outputGood(req);
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
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            if (resp->getBody() == "staticApi,hello!!")
            {
                outputGood(req);
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

    app().loop()->runAfter(0.5, [=]() mutable {
        req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setPath("/api/v1/apitest/static");
        client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                if (resp->getBody() == "staticApi,hello!!")
                {
                    outputGood(req);
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
    });

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/apitest/get/111");
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            if (resp->getBody().length() == 5123)
            {
                outputGood(req);
            }
            else
            {
                //LOG_DEBUG << resp->getBody();
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

    /// Test gzip
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->addHeader("accept-encoding", "gzip");
    req->setPath("/api/v1/apitest/get/111");
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            if (resp->getBody().length() == 1754)
            {
                outputGood(req);
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

    /// Test static function
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/v1/handle11/11/22/?p3=33&p4=44");
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            if (resp->getBody().find("<td>int p1</td>\n        <td>11</td>") != std::string::npos &&
                resp->getBody().find("<td>int p4</td>\n        <td>44</td>") != std::string::npos &&
                resp->getBody().find("<td>string p2</td>\n        <td>22</td>") != std::string::npos &&
                resp->getBody().find("<td>string p3</td>\n        <td>33</td>") != std::string::npos)
            {
                outputGood(req);
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
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            if (resp->getBody().find("<td>a</td>\n        <td>111</td>") != std::string::npos &&
                resp->getBody().find("<td>b</td>\n        <td>222.000000</td>") != std::string::npos)
            {
                outputGood(req);
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
    client->sendRequest(req, [=](ReqResult result, const HttpResponsePtr &resp) {
        if (result == ReqResult::Ok)
        {
            if (resp->getBody().find("<td>int p1</td>\n        <td>111</td>") != std::string::npos &&
                resp->getBody().find("<td>int p4</td>\n        <td>444</td>") != std::string::npos &&
                resp->getBody().find("<td>string p3</td>\n        <td>333</td>") != std::string::npos &&
                resp->getBody().find("<td>string p2</td>\n        <td></td>") != std::string::npos)
            {
                outputGood(req);
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

    app().run();
}