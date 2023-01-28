#pragma once
#include <drogon/HttpController.h>
using namespace drogon;
namespace api
{
namespace v1
{
class CoroTest : public drogon::HttpController<CoroTest>
{
  public:
    METHOD_LIST_BEGIN
    METHOD_ADD(CoroTest::get, "/get", Get);
    METHOD_ADD(CoroTest::get_with_param, "/get_with_param/{name}", Get);
    METHOD_ADD(CoroTest::get_with_param2, "/get_with_param2/{name}", Get);
    METHOD_ADD(CoroTest::get2, "/get2", Get);
    METHOD_ADD(CoroTest::get2, "/delay", Get, "CoroFilter");
    METHOD_ADD(CoroTest::this_will_fail, "/this_will_fail", Get);
    METHOD_ADD(CoroTest::this_will_fail2, "/this_will_fail2", Get);
    METHOD_LIST_END

    Task<> get(HttpRequestPtr req,
               std::function<void(const HttpResponsePtr &)> callback);
    Task<> get_with_param(HttpRequestPtr req,
                          std::function<void(const HttpResponsePtr &)> callback,
                          std::string name);
    Task<HttpResponsePtr> get_with_param2(HttpRequestPtr req, std::string name);
    Task<HttpResponsePtr> get2(HttpRequestPtr req);
    Task<> this_will_fail(
        HttpRequestPtr req,
        std::function<void(const HttpResponsePtr &)> callback);
    Task<HttpResponsePtr> this_will_fail2(HttpRequestPtr req);
};
}  // namespace v1
}  // namespace api
