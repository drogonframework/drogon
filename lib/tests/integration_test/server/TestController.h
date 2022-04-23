#pragma once
#include <drogon/HttpSimpleController.h>
#include <drogon/IOThreadStorage.h>
using namespace drogon;
namespace example
{
class TestController : public drogon::HttpSimpleController<TestController>
{
  public:
    virtual void asyncHandleHttpRequest(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
    // list path definations here;
    // PATH_ADD("/path","filter1","filter2",...);
    PATH_ADD("/", Get);
    PATH_ADD("/Test", "nonFilter");
    PATH_ADD("/tpost", Post, Options);
    PATH_ADD("/slow", "TimeFilter", Get);
    PATH_LIST_END
    TestController()
    {
        LOG_DEBUG << "TestController constructor";
    }

  private:
    drogon::IOThreadStorage<int> threadIndex_;
};
}  // namespace example
