#pragma once
#include <drogon/HttpSimpleController.h>
#include <string>

using namespace drogon;

class BeginAdviceTest : public drogon::HttpSimpleController<BeginAdviceTest>
{
  public:
    void asyncHandleHttpRequest(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
    // list path definitions here;
    // PATH_ADD("/path","filter1","filter2",...);
    PATH_ADD("/test_begin_advice", Get);

    PATH_LIST_END
    BeginAdviceTest()
    {
        LOG_DEBUG << "BeginAdviceTest constructor";
    }

    static void setContent(const std::string &content)
    {
        content_ = content;
    }

  private:
    static std::string content_;
};
