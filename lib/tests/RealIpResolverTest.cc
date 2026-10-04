#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/plugins/RealIpResolver.h>
#include <drogon/drogon.h>
#include <drogon/HttpTypes.h>

using namespace drogon;

DROGON_TEST(RealIpResolver)
{
    auto client =
        HttpClient::newHttpClient("http://127.0.0.1:8017",
                                  HttpAppFramework::instance().getLoop());

    auto newRequest = []() {
        auto req = HttpRequest::newHttpRequest();
        req->setPath("/RealIpController/my-ip");
        return req;
    };

    // 1. No headers
    {
        auto req = newRequest();
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "127.0.0.1");
            });
    }
    // 2. header only contains real ip
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "1.1.1.1");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "1.1.1.1");
            });
    }
    // 3. Ip with port
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "1.1.1.1:7777");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "1.1.1.1");
            });
    }
    // 3. multiple ips
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for",
                       "2.2.2.2,1.1.1.1:7001,  172.16.0.100:7002,127.0.0.1");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "1.1.1.1");
            });
    }
    // 4. Ignore error in header
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for",
                       "2.2.2.2,1.1.1.1:7001, wrong,9.9.9.9");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "1.1.1.1");
            });
    }
    // Note on the ipv6 cases below: the header is scanned from the right, so
    // the rightmost entry is the one closest to this server. The first entry
    // from the right that is not a trusted proxy wins. trust_ips contains
    // "::1" and "2001:db8::/32", so anything under 2001:db8::/32 is treated as
    // a trusted proxy and skipped - the untrusted addresses below therefore
    // deliberately use the distinct 2001:dba:: prefix.
    //
    // 5. Bare ipv6 in the header
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "2001:dba::1");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "2001:dba::1");
            });
    }
    // 6. Bracketed ipv6 with port
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "[2001:dba::2]:7777");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "2001:dba::2");
            });
    }
    // 7. Bracketed ipv6 without port
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "[2001:dba::3]");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "2001:dba::3");
            });
    }
    // 8. An ipv6 entry inside a trusted cidr is skipped: 2001:db8::99 matches
    //    the trusted 2001:db8::/32, so the untrusted 2001:dba::7 before it is
    //    the real client.
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "2001:dba::7,2001:db8::99");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "2001:dba::7");
            });
    }
    // 9. Every entry is a trusted proxy (::1 exact, 2001:db8::99 by cidr), so
    //    the resolver falls back to the peer address.
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "2001:db8::99,::1");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "127.0.0.1");
            });
    }
    // 10. Mixed v4/v6 header where the trusted proxy is ipv6 and the real
    //     client is ipv4
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "3.3.3.3:9000,2001:db8::1");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "3.3.3.3");
            });
    }
    // 11. ipv4-mapped ipv6 address is still a valid v6 address
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "::ffff:4.4.4.4");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "::ffff:4.4.4.4");
            });
    }
    // 12. A prefix length that is not a multiple of 8 must compare the partial
    //     byte too. 2001:dbd::/33 keeps bit 33, which is the top bit of the
    //     fifth byte. 2001:dbd::9 has that bit clear, so it is inside the
    //     trusted block and the resolver falls back to the peer address.
    //     (If the partial byte were ignored, this returned 2001:dbd::9.)
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "2001:dbd::9");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "127.0.0.1");
            });
    }
    // 13. 2001:dbd:8000::5 has bit 33 set, so it is outside 2001:dbd::/33 and
    //     must be reported as the real client. (If the partial byte were
    //     ignored, this matched and returned the peer address instead.)
    {
        auto req = newRequest();
        req->addHeader("x-forwarded-for", "2001:dbd:8000::5");
        client->sendRequest(
            req, [TEST_CTX](ReqResult res, const HttpResponsePtr &resp) {
                REQUIRE(res == ReqResult::Ok);
                CHECK(resp->getStatusCode() == HttpStatusCode::k200OK);
                CHECK(resp->contentType() == drogon::CT_TEXT_PLAIN);
                CHECK(resp->body() == "2001:dbd:8000::5");
            });
    }
};

class RealIpController : public drogon::HttpController<RealIpController>
{
  public:
    METHOD_LIST_BEGIN
    METHOD_ADD(RealIpController::getRealIp, "/my-ip", Get);
    METHOD_LIST_END

    void getRealIp(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto addr = req->attributes()->get<trantor::InetAddress>("real-ip");
        auto resp = HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
        resp->setBody(addr.toIp());
        callback(resp);
    }
};

// -- main
int main(int argc, char **argv)
{
    trantor::Logger::setLogLevel(trantor::Logger::kInfo);
    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();

    std::stringstream ss;
    ss << R"({
    "listeners": [
        {
            "address": "0.0.0.0",
            "port": 8017
        }
    ],
    "plugins": [
        {
            "name": "drogon::plugin::RealIpResolver",
            "config": {
                "trust_ips": ["127.0.0.1", "172.16.0.0/12", "9.9.9.9/32",
                              "::1", "2001:db8::/32", "2001:dbd::/33"],
                "from_header": "x-forwarded-for",
                "attribute_key": "real-ip"
            }
        }
    ]
})";
    Json::Value config;
    ss >> config;

    std::thread thr([&]() {
        app().loadConfigJson(config);
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
