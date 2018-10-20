#pragma once
#include <drogon/HttpApiController.h>
using namespace drogon;
namespace api
{
namespace v1
{
class ApiTest : public drogon::HttpApiController<ApiTest>
{
  public:
    METHOD_LIST_BEGIN
    //use METHOD_ADD to add your custom processing function here;
    registerMethod(&ApiTest::get, "/get/{2}/{1}", {},{"drogon::GetFilter"});                  //path will be /api/v1/apitest/get/{arg2}/{arg1}
    registerMethod(&ApiTest::your_method_name, "/{1}/List?P2={2}", {HttpRequest::Get}); //path will be /api/v1/apitest/{arg1}/list
    registerMethod(&ApiTest::staticApi, "/static",{HttpRequest::Get,HttpRequest::Post});
    METHOD_ADD(ApiTest::get2, "/get/{1}", "drogon::GetFilter");
    METHOD_LIST_END
    //your declaration of processing function maybe like this:
    void get(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback, int p1, std::string &&p2);
    void your_method_name(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback, double p1, int p2) const;
    void staticApi(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback);
    void get2(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback, std::string &&p1);
};
} // namespace v1
} // namespace api
