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
    // use METHOD_ADD to add your custom processing function here;
    METHOD_ADD(ApiTest::rootGet,
               "",
               Get,
               Options,
               "drogon::LocalHostFilter",
               "drogon::IntranetIpFilter");
    METHOD_ADD(ApiTest::rootPost, "", Post, Options);
    METHOD_ADD(ApiTest::get,
               "/get/{2:p2}/{1}",
               Get);  // path is /api/v1/apitest/get/{arg2}/{arg1}
    METHOD_ADD(ApiTest::your_method_name,
               "/{PI}/List?P2={}",
               Get);  // path is /api/v1/apitest/{arg1}/list
    METHOD_ADD(ApiTest::staticApi, "/static", Get, Options);  // CORS
    METHOD_ADD(ApiTest::staticApi, "/static", Post, Put, Delete);
    METHOD_ADD(ApiTest::get2,
               "/get/{1}",
               Get);  // path is /api/v1/apitest/get/{arg1}
    ADD_METHOD_TO(ApiTest::get2,
                  "/absolute/{1}",
                  Get);  // path is /absolute/{arg1}
    METHOD_ADD(ApiTest::jsonTest, "/json", Post);
    METHOD_ADD(ApiTest::formTest, "/form", Post);
    METHOD_ADD(ApiTest::attributesTest, "/attrs", Get);
    METHOD_LIST_END

    void get(const HttpRequestPtr &req,
             std::function<void(const HttpResponsePtr &)> &&callback,
             int p1,
             std::string &&p2);
    void your_method_name(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        double p1,
        int p2) const;
    void staticApi(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
    void get2(const HttpRequestPtr &req,
              std::function<void(const HttpResponsePtr &)> &&callback,
              std::string &&p1);
    void rootGet(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
    void rootPost(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
    void jsonTest(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
    void formTest(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
    void attributesTest(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback);

  public:
    ApiTest()
    {
        LOG_DEBUG << "ApiTest constructor!";
    }
};
}  // namespace v1
}  // namespace api
