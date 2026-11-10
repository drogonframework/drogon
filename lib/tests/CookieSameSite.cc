#include <drogon/Cookie.h>
#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>

using namespace drogon;

struct CookieSameSiteSequence
{
    CookieSameSiteSequence()
    {
        i = valid_sameSite_values.begin();
        sessionCookie.setKey("JSESSIONID");
    }

    std::vector<std::string> valid_sameSite_values{"Null",
                                                   "Lax",
                                                   "None",
                                                   "Strict"};

    std::vector<std::string>::const_iterator i;
    Cookie sessionCookie;
};

DROGON_TEST(CookieSameSite)
{
    auto client =
        HttpClient::newHttpClient("https://127.0.0.1:8855",
                                  HttpAppFramework::instance().getLoop(),
                                  false,
                                  false);
    auto req = HttpRequest::newHttpRequest();

    CookieSameSiteSequence seq;

    while (seq.i != seq.valid_sameSite_values.end())
    {
        std::promise<void> p1;
        std::future<void> f1 = p1.get_future();
        req->setPath(std::string("/CookieSameSiteController/set/") + *seq.i);
        if (seq.sessionCookie.getValue() != "")
        {
            // add session cookie
            req->addCookie(seq.sessionCookie.key(), seq.sessionCookie.value());
        }  // endif

        client->sendRequest(
            req,
            [TEST_CTX, &seq, &p1](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                REQUIRE(resp != nullptr);

                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == CT_APPLICATION_JSON);

                auto json = resp->getJsonObject();
                auto cookie = resp->getCookie("JSESSIONID");

                LOG_INFO << "Client: cookie-value == " << cookie.value()
                         << ", requested value == " << (*seq.i)
                         << ", new  value == "
                         << (*json)["new value"].asString()
                         << ", received value == "
                         << Cookie::convertSameSite2String(
                                cookie.getSameSite());
                seq.sessionCookie = resp->getCookie("JSESSIONID");
                CHECK(*seq.i ==
                      Cookie::convertSameSite2String(cookie.getSameSite()));

                p1.set_value();
            });
        f1.get();
        seq.i++;
    }
}

DROGON_TEST(SessionRotationInvalidatesOldId)
{
    auto client =
        HttpClient::newHttpClient("https://127.0.0.1:8855",
                                  HttpAppFramework::instance().getLoop(),
                                  false,
                                  false);
    const auto request = [&client](const std::string &path,
                                   const std::string &sessionId) {
        auto req = HttpRequest::newHttpRequest();
        req->setPath("/CookieSameSiteController/session/" + path);
        if (!sessionId.empty())
            req->addCookie("JSESSIONID", sessionId);
        return client->sendRequest(req, 5.0);
    };

    auto seed = request("seed", "");
    REQUIRE(seed.first == ReqResult::Ok);
    REQUIRE(seed.second != nullptr);
    const auto oldId = seed.second->getCookie("JSESSIONID").value();
    REQUIRE(!oldId.empty());

    auto login = request("login", oldId);
    REQUIRE(login.first == ReqResult::Ok);
    REQUIRE(login.second != nullptr);
    const auto newId = login.second->getCookie("JSESSIONID").value();
    REQUIRE(!newId.empty());
    CHECK(newId != oldId);

    auto newSession = request("read", newId);
    REQUIRE(newSession.first == ReqResult::Ok);
    REQUIRE(newSession.second != nullptr);
    CHECK(newSession.second->getBody() == "user");

    auto oldSession = request("read", oldId);
    REQUIRE(oldSession.first == ReqResult::Ok);
    REQUIRE(oldSession.second != nullptr);
    CHECK(oldSession.second->getBody().empty());
}
