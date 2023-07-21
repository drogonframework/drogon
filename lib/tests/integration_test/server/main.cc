
#include "BeginAdviceTest.h"
#include "CustomCtrl.h"
#include "CustomHeaderFilter.h"
#include "DigestAuthFilter.h"

#include <drogon/drogon.h>
#include <iostream>
#include <string>
#include <vector>

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
        std::unordered_map<std::string,
                           std::string,
                           utils::internal::SafeStringHash>
            para;
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
        std::unordered_map<std::string,
                           std::string,
                           utils::internal::SafeStringHash>
            para;
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
        std::unordered_map<std::string,
                           std::string,
                           utils::internal::SafeStringHash>
            para;
        para["p1"] = std::to_string(p1);
        para["p2"] = std::to_string(p2);
        data.insert("parameters", para);
        auto res = HttpResponse::newHttpViewResponse("ListParaView", data);
        callback(res);
    }
};

class C : public drogon::HttpController<C>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(C::priv, "/priv/resource", Get, "DigestAuthFilter");
    METHOD_LIST_END
    void priv(const HttpRequestPtr &req,
              std::function<void(const HttpResponsePtr &)> &&callback) const
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody("<P>private content, only for authenticated users</P>");
        callback(resp);
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
        std::unordered_map<std::string,
                           std::string,
                           utils::internal::SafeStringHash>
            para;
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
        std::unordered_map<std::string,
                           std::string,
                           utils::internal::SafeStringHash>
            para;
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

namespace drogon
{
template <>
string_view fromRequest(const HttpRequest &req)
{
    return req.body();
}
}  // namespace drogon

/// Some examples in the main function show some common functions of drogon. In
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
            .setSSLFiles("server.crt", "server.key")
            .addListener("0.0.0.0", 8849, true);
    }
    // Class function example
    app().registerHandler("/api/v1/handle1/{}/{}/?p3={}&p4={}", &A::handle);
    app().registerHandler(
        "/api/v1/handle11/{int p1}/{string p2}/?p3={string p3}&p4={int p4}",
        &A::staticHandle);
    // Lambda example
    app().registerHandler(
        "/api/v1/handle2/{int a}/{float b}",
        [](const HttpRequestPtr &req,
           std::function<void(const HttpResponsePtr &)> &&callback,
           int a,    // here the `a` parameter is converted from the number 1
                     // parameter in the path.
           float b,  // here the `b` parameter is converted from the number 2
                     // parameter in the path.
           string_view &&body,  // here the `body` parameter is converted from
                                // req->as<string_view>();
           const std::shared_ptr<Json::Value>
               &jsonPtr  // here the `jsonPtr` parameter is converted from
                         // req->as<std::shared_ptr<Json::Value>>();
        ) {
            HttpViewData data;
            data.insert("title", std::string("ApiTest::get"));
            std::unordered_map<std::string,
                               std::string,
                               utils::internal::SafeStringHash>
                para;
            para["a"] = std::to_string(a);
            para["b"] = std::to_string(b);
            data.insert("parameters", para);
            auto res = HttpResponse::newHttpViewResponse("ListParaView", data);
            callback(res);
            LOG_DEBUG << body.data();
            assert(!jsonPtr);
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
    app().registerHandler("/api/v1/handle4/{4:p4}/{3:p3}/{1:p1}", func);
    app().registerHandler(
        "/api/v1/this_will_fail",
        [](const HttpRequestPtr &req,
           std::function<void(const HttpResponsePtr &)> &&callback) {
            throw std::runtime_error("this should fail");
        });

    app().setDocumentRoot("./");
    app().enableSession(60);

    std::map<std::string, std::string> config_credentials;
    std::string realm("drogonRealm");
    std::string opaque("drogonOpaque");
    // Load configuration
    app().loadConfigFile("config.example.json");
    app().setImplicitPageEnable(true);
    app().setImplicitPage("page.html");
    auto &json = app().getCustomConfig();
    if (json.empty())
    {
        std::cout << "empty custom config!" << std::endl;
    }
    else
    {
        if (!json["realm"].empty())
        {
            realm = json["realm"].asString();
        }
        if (!json["opaque"].empty())
        {
            opaque = json["opaque"].asString();
        }
        for (auto &&i : json["credentials"])
        {
            config_credentials[i["user"].asString()] = i["password"].asString();
        }
    }

    // Install Digest Authentication Filter using custom config credentials,
    // used by C HttpController (/C/priv/resource)
    auto auth_filter =
        std::make_shared<DigestAuthFilter>(config_credentials, realm, opaque);
    app().registerFilter(auth_filter);

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
                CT_TEXT_PLAIN, "content-type: text/plain\r\n");
            return resp;
        }
        return nullResp;
    });
    app().registerSessionStartAdvice([](const std::string &sessionId) {
        LOG_DEBUG << "session start:" << sessionId;
    });
    app().registerSessionDestroyAdvice([](const std::string &sessionId) {
        LOG_DEBUG << "session destroy:" << sessionId;
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
            case Patch:
                std::cout << " (PATCH) ";
                break;
            default:
                break;
        }
        std::cout << std::get<2>(info) << std::endl;
    }
    auto resp = HttpResponse::newFileResponse("index.html");
    resp->setExpiredTime(0);
    app().setCustom404Page(resp);
    app().addListener("0.0.0.0", 0);
    app().enableCompressedRequest(true);
    app().registerBeginningAdvice([]() {
        auto addresses = app().getListeners();
        for (auto &address : addresses)
        {
            LOG_INFO << address.toIpPort() << " LISTEN";
        }
    });
    app().registerCustomExtensionMime("md", "text/markdown");
    app().setFileTypes({"md", "html", "jpg", "cc", "txt"});
    std::cout << "Date: "
              << std::string{drogon::utils::getHttpFullDate(
                     trantor::Date::now())}
              << std::endl;

    app().registerBeginningAdvice(
        []() { BeginAdviceTest::setContent("DrogonReady"); });

    app().run();
}
