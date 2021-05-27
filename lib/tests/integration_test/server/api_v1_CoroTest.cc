#include "api_v1_CoroTest.h"
using namespace api::v1;

Task<> CoroTest::get(HttpRequestPtr req,
                     std::function<void(const HttpResponsePtr &)> callback)
{
    // Force co_await to test awaiting works
    co_await drogon::sleepCoro(
        trantor::EventLoop::getEventLoopOfCurrentThread(),
        std::chrono::milliseconds(100));

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("DEADBEEF");
    callback(resp);
    co_return;
}

Task<HttpResponsePtr> CoroTest::get2(HttpRequestPtr req)
{
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("BADDBEEF");
    co_return resp;
}

Task<> CoroTest::this_will_fail(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback)
{
    throw std::runtime_error("This is an excpected exception");
    callback(HttpResponse::newHttpResponse());
}

Task<HttpResponsePtr> CoroTest::this_will_fail2(HttpRequestPtr req)
{
    throw std::runtime_error("This is an excpected exception");
    co_return HttpResponse::newHttpResponse();
}
