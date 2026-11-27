#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpController.h>
#include <drogon/Cookie.h>

#include <trantor/utils/Logger.h>

using namespace drogon;
using namespace trantor;

class CookieSameSiteController
    : public drogon::HttpController<CookieSameSiteController>
{
  public:
    static const char *SESSION_SAME_SITE;

    METHOD_LIST_BEGIN
    METHOD_ADD(CookieSameSiteController::set, "/set/{newSameSite}", Get);
    METHOD_ADD(CookieSameSiteController::seedSession, "/session/seed", Get);
    METHOD_ADD(CookieSameSiteController::loginSession, "/session/login", Get);
    METHOD_ADD(CookieSameSiteController::readSession, "/session/read", Get);
    METHOD_LIST_END

    void seedSession(const HttpRequestPtr &,
                     std::function<void(const HttpResponsePtr &)> &&callback)
    {
        callback(HttpResponse::newHttpResponse());
    }

    void loginSession(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback)
    {
        req->session()->insert("rotation_test_user", std::string("user"));
        req->session()->changeSessionIdToClient();
        callback(HttpResponse::newHttpResponse());
    }

    void readSession(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(req->session()->get<std::string>("rotation_test_user"));
        callback(resp);
    }

    void set(const HttpRequestPtr &req,
             std::function<void(const HttpResponsePtr &)> &&callback,
             std::string &&newSameSite)
    {
        std::string old_session_same_site = "Null";
        if (req->session()->find(SESSION_SAME_SITE))
        {
            old_session_same_site =
                req->session()->get<std::string>(SESSION_SAME_SITE);
        }
        LOG_INFO << "Server: new sameSite == " << newSameSite
                 << ", old sameSite == " << old_session_same_site;
        drogon::HttpAppFramework::instance().enableSession(
            0, Cookie::convertString2SameSite(newSameSite));

        req->session()->modify<std::string>(
            SESSION_SAME_SITE,
            [newSameSite](std::string &sameSite) { sameSite = newSameSite; });
        req->session()->changeSessionIdToClient();

        Json::Value json;
        json["result"] = "ok";
        json["old value"] = old_session_same_site;
        json["new value"] = newSameSite;

        auto resp = HttpResponse::newHttpJsonResponse(std::move(json));
        callback(resp);
    }
};

const char *CookieSameSiteController::SESSION_SAME_SITE{"session_same_site"};

// -- main
int main(int argc, char **argv)
{
    trantor::Logger::setLogLevel(trantor::Logger::kInfo);
    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();

    std::thread thr([&]() {
        app()
            .setSSLFiles("server.crt", "server.key")
            .addListener("0.0.0.0", 8855, true)
            .enableSession(60);
        app().getLoop()->queueInLoop([&p1]() { p1.set_value(); });
        app().run();
    });

    f1.get();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int testStatus = test::run(argc, argv);
    app().getLoop()->queueInLoop([]() { app().quit(); });
    thr.join();
    return testStatus;
}
