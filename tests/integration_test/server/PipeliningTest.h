#pragma once
#include <drogon/HttpSimpleController.h>
using namespace drogon;
class PipeliningTest : public drogon::HttpSimpleController<PipeliningTest>
{
  public:
    virtual void asyncHandleHttpRequest(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
    // list path definitions here;
    PATH_ADD("/pipe", Get);
    PATH_LIST_END
};
