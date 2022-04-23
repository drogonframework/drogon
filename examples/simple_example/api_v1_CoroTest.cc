#include "api_v1_CoroTest.h"
using namespace api::v1;

Task<> CoroTest::get(HttpRequestPtr req,
                     std::function<void(const HttpResponsePtr &)> callback)
{
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
