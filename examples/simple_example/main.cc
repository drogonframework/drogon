#include <iostream>
#include <drogon/drogon.h>
#include <vector>
#include <string>

using namespace drogon;
class A : public DrObjectBase
{
  public:
    void handle(const HttpRequestPtr &req,
                const std::function<void(const HttpResponsePtr &)> &callback,
                int p1, const std::string &p2, const std::string &p3, int p4) const
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
    static void staticHandle(const HttpRequestPtr &req,
                             const std::function<void(const HttpResponsePtr &)> &callback,
                             int p1, const std::string &p2, const std::string &p3, int p4)
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
    void operator()(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback, int p1, int p2)
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
    METHOD_ADD(Test::get, "get/{2}/{1}", Get); //path is /api/v1/test/get/{arg2}/{arg1}
    METHOD_ADD(Test::list, "/{2}/info", Get);  //path is /api/v1/test/{arg2}/info
    METHOD_LIST_END
    void get(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback, int p1, int p2) const
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
    void list(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback, int p1, int p2) const
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
} // namespace v1
} // namespace api

using namespace std::placeholders;
using namespace drogon;
int main()
{

    std::cout << banner << std::endl;
    //app().addListener("::1", 8848);
    app().addListener("0.0.0.0", 8848);
#ifdef USE_OPENSSL
    //https
    drogon::HttpAppFramework::instance().setSSLFiles("server.pem", "server.pem");
    //drogon::HttpAppFramework::instance().addListener("::1", 8849, true);
    drogon::HttpAppFramework::instance().addListener("0.0.0.0", 8849, true);
#endif
    //app().setThreadNum(4);
    //    trantor::Logger::setLogLevel(trantor::Logger::TRACE);
    //class function
    app().registerHandler("/api/v1/handle1/{1}/{2}/?p3={3}&p4={4}", &A::handle);
    app().registerHandler("/api/v1/handle11/{1}/{2}/?p3={3}&p4={4}", &A::staticHandle);
    //lambda example
    app().registerHandler("/api/v1/handle2/{1}/{2}", [](const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback, int a, float b) {
        // LOG_DEBUG << "int a=" << a;
        // LOG_DEBUG << "float b=" << b;
        HttpViewData data;
        data.insert("title", std::string("ApiTest::get"));
        std::map<std::string, std::string> para;
        para["a"] = std::to_string(a);
        para["b"] = std::to_string(b);
        data.insert("parameters", para);
        auto res = HttpResponse::newHttpViewResponse("ListParaView", data);
        callback(res);
    });

    B b;
    //functor example
    app().registerHandler("/api/v1/handle3/{1}/{2}", b);

    A tmp;
    std::function<void(const HttpRequestPtr &, const std::function<void(const HttpResponsePtr &)> &, int, const std::string &, const std::string &, int)>
        func = std::bind(&A::handle, &tmp, _1, _2, _3, _4, _5, _6);
    //api example for std::function
    app().registerHandler("/api/v1/handle4/{4}/{3}/{1}", func);

    app().setDocumentRoot("./");
    app().enableSession(60);
    //start app framework
    //drogon::HttpAppFramework::instance().enableDynamicViewsLoading({"/tmp/views"});
    app().loadConfigFile("config.example.json");
    auto &json = app().getCustomConfig();
    if(json.empty())
    {
        std::cout << "empty custom config!" << std::endl;
    }
    app().run();
}
