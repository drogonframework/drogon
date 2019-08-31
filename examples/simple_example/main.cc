
#include "CustomCtrl.h"
#include "CustomHeaderFilter.h"
#include <drogon/drogon.h>
#include <vector>
#include <string>
#include <iostream>

using namespace drogon;
using namespace std::chrono_literals;
class A : public DrObjectBase
{
  public:
    void handle(const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback,
                int p1,
                const std::string &p2,
                const std::string &p3,
                int p4) const
    {
        HttpViewData data;
        data.insert("title", std::string("ApiTest::get"));
        std::map<std::string, std::string> para;
        para["int p1"] = std::to_string(p1);
        para["string p2"] = p2;
        para["string p3"] = p3;
        para["int p4"] = std::to_string(p4);

        data.insert("parameters", para);
        auto res = HttpResponse::newHttpViewResponse("ListParaView", data);
        callback(res);
    }
    static void staticHandle(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        int p1,
        const std::string &p2,
        const std::string &p3,
        int p4)
    {
        HttpViewData data;
        data.insert("title", std::string("ApiTest::get"));
        std::map<std::string, std::string> para;
        para["int p1"] = std::to_string(p1);
        para["string p2"] = p2;
        para["string p3"] = p3;
        para["int p4"] = std::to_string(p4);

        data.insert("parameters", para);
        auto res = HttpResponse::newHttpViewResponse("ListParaView", data);
        callback(res);
    }
};
class B : public DrObjectBase
{
  public:
    void operator()(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback,
                    int p1,
                    int p2)
    {
        HttpViewData data;
        data.insert("title", std::string("ApiTest::get"));
        std::map<std::string, std::string> para;
        para["p1"] = std::to_string(p1);
        para["p2"] = std::to_string(p2);
        data.insert("parameters", para);
        auto res = HttpResponse::newHttpViewResponse("ListParaView", data);
        callback(res);
    }
};
namespace api
{
namespace v1
{
class Test : public HttpController<Test>
{
  public:
    METHOD_LIST_BEGIN
    METHOD_ADD(Test::get,
               "get/{2}/{1}",
               Get);  // path is /api/v1/test/get/{arg2}/{arg1}
    METHOD_ADD(Test::list,
               "/{2}/info",
               Get);  // path is /api/v1/test/{arg2}/info
    METHOD_LIST_END
    void get(const HttpRequestPtr &req,
             std::function<void(const HttpResponsePtr &)> &&callback,
             int p1,
             int p2) const
    {
        HttpViewData data;
        data.insert("title", std::string("ApiTest::get"));
        std::map<std::string, std::string> para;
        para["p1"] = std::to_string(p1);
        para["p2"] = std::to_string(p2);
        data.insert("parameters", para);
        auto res = HttpResponse::newHttpViewResponse("ListParaView", data);
        callback(res);
    }
    void list(const HttpRequestPtr &req,
              std::function<void(const HttpResponsePtr &)> &&callback,
              int p1,
              int p2) const
    {
        HttpViewData data;
        data.insert("title", std::string("ApiTest::get"));
        std::map<std::string, std::string> para;
        para["p1"] = std::to_string(p1);
        para["p2"] = std::to_string(p2);
        data.insert("parameters", para);
        auto res = HttpResponse::newHttpViewResponse("ListParaView", data);
        callback(res);
    }
};
}  // namespace v1
}  // namespace api

using namespace std::placeholders;
using namespace drogon;

/// Some examples in the main function some common functions of drogon. In
/// practice, we don't need such a lengthy main function.
int main()
{
    std::cout << banner << std::endl;
    // app().addListener("::1", 8848); //ipv6
    app().addListener("0.0.0.0", 8848);
    // https
    if (app().supportSSL())
    {
        drogon::app()
            .setSSLFiles("server.pem", "server.pem")
            .addListener("0.0.0.0", 8849, true);
    }
    // Class function example
    app().registerHandler("/api/v1/handle1/{1}/{2}/?p3={3}&p4={4}", &A::handle);
    app().registerHandler("/api/v1/handle11/{1}/{2}/?p3={3}&p4={4}",
                          &A::staticHandle);
    // Lambda example
    app().registerHandler(
        "/api/v1/handle2/{1}/{2}",
        [](const HttpRequestPtr &req,
           std::function<void(const HttpResponsePtr &)> &&callback,
           int a,
           float b) {
            HttpViewData data;
            data.insert("title", std::string("ApiTest::get"));
            std::map<std::string, std::string> para;
            para["a"] = std::to_string(a);
            para["b"] = std::to_string(b);
            data.insert("parameters", para);
            auto res = HttpResponse::newHttpViewResponse("ListParaView", data);
            callback(res);
        });

    // Functor example
    B b;
    app().registerHandler("/api/v1/handle3/{1}/{2}", b);

    // API example for std::function
    A tmp;
    std::function<void(const HttpRequestPtr &,
                       std::function<void(const HttpResponsePtr &)> &&,
                       int,
                       const std::string &,
                       const std::string &,
                       int)>
        func = std::bind(&A::handle, &tmp, _1, _2, _3, _4, _5, _6);
    app().registerHandler("/api/v1/handle4/{4}/{3}/{1}", func);

    app().setDocumentRoot("./");
    app().enableSession(60);

    // Load configuration
    app().loadConfigFile("config.example.json");
    auto &json = app().getCustomConfig();
    if (json.empty())
    {
        std::cout << "empty custom config!" << std::endl;
    }

    // Install custom controller
    auto ctrlPtr = std::make_shared<CustomCtrl>("Hi");
    app().registerController(ctrlPtr);

    // Install custom filter
    auto filterPtr =
        std::make_shared<CustomHeaderFilter>("custom_header", "yes");
    app().registerFilter(filterPtr);
    app().setIdleConnectionTimeout(30s);

    // AOP example
    app().registerBeginningAdvice(
        []() { LOG_DEBUG << "Event loop is running!"; });
    app().registerNewConnectionAdvice([](const trantor::InetAddress &peer,
                                         const trantor::InetAddress &local) {
        LOG_DEBUG << "New connection: " << peer.toIpPort() << "-->"
                  << local.toIpPort();
        return true;
    });
    app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr &req,
                                      drogon::AdviceCallback &&acb,
                                      drogon::AdviceChainCallback &&accb) {
        LOG_DEBUG << "preRouting1";
        accb();
    });
    app().registerPostRoutingAdvice([](const drogon::HttpRequestPtr &req,
                                       drogon::AdviceCallback &&acb,
                                       drogon::AdviceChainCallback &&accb) {
        LOG_DEBUG << "postRouting1";
        LOG_DEBUG << "Matched path=" << req->matchedPathPatternData();
        for (auto &cookie : req->cookies())
        {
            LOG_DEBUG << "cookie: " << cookie.first << "=" << cookie.second;
        }
        accb();
    });
    app().registerPreHandlingAdvice([](const drogon::HttpRequestPtr &req,
                                       drogon::AdviceCallback &&acb,
                                       drogon::AdviceChainCallback &&accb) {
        LOG_DEBUG << "preHandling1";
        accb();
    });
    app().registerPostHandlingAdvice([](const drogon::HttpRequestPtr &,
                                        const drogon::HttpResponsePtr &resp) {
        LOG_DEBUG << "postHandling1";
        resp->addHeader("Access-Control-Allow-Origin", "*");
    });
    app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr &req) {
        LOG_DEBUG << "preRouting observer";
    });
    app().registerPostRoutingAdvice([](const drogon::HttpRequestPtr &req) {
        LOG_DEBUG << "postRouting observer";
    });
    app().registerPreHandlingAdvice([](const drogon::HttpRequestPtr &req) {
        LOG_DEBUG << "preHanding observer";
    });
    app().registerSyncAdvice([](const HttpRequestPtr &req) -> HttpResponsePtr {
        static const HttpResponsePtr nullResp;
        if (req->path() == "/plaintext")
        {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody("Hello, World!");
            resp->setContentTypeCodeAndCustomString(
                CT_TEXT_PLAIN, "Content-Type: text/plain\r\n");
            return resp;
        }
        return nullResp;
    });
    // Output information of all handlers
    auto handlerInfo = app().getHandlersInfo();
    for (auto &info : handlerInfo)
    {
        std::cout << std::get<0>(info);
        switch (std::get<1>(info))
        {
            case Get:
                std::cout << " (GET) ";
                break;
            case Post:
                std::cout << " (POST) ";
                break;
            case Delete:
                std::cout << " (DELETE) ";
                break;
            case Put:
                std::cout << " (PUT) ";
                break;
            case Options:
                std::cout << " (OPTIONS) ";
                break;
            case Head:
                std::cout << " (Head) ";
                break;
            default:
                break;
        }
        std::cout << std::get<2>(info) << std::endl;
    }

    app().run();
}
