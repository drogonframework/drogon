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
    METHOD_ADD(CoroTest::get2, "/get2", Get);
    METHOD_LIST_END

    Task<> get(HttpRequestPtr req,
               std::function<void(const HttpResponsePtr &)> callback);
    Task<HttpResponsePtr> get2(HttpRequestPtr req);
};
}  // namespace v1
}  // namespace api
