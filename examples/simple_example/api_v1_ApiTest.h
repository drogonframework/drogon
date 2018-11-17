#pragma once
#include <drogon/HttpController.h>
using namespace drogon;
namespace api
{
namespace v1
{
class ApiTest : public drogon::HttpController<ApiTest>
{
public:
  METHOD_LIST_BEGIN
  //use METHOD_ADD to add your custom processing function here;
  METHOD_ADD(ApiTest::get, "/get/{2}/{1}", "drogon::GetFilter");               //path will be /api/v1/apitest/get/{arg2}/{arg1}
  METHOD_ADD(ApiTest::your_method_name, "/{1}/List?P2={2}", Get); //path will be /api/v1/apitest/{arg1}/list
  METHOD_ADD(ApiTest::staticApi, "/static", Get, Post);
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
