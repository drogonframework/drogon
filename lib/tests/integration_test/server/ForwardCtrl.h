#pragma once
#include <drogon/HttpSimpleController.h>
using namespace drogon;

class ForwardCtrl : public drogon::HttpSimpleController<ForwardCtrl>
{
  public:
    void asyncHandleHttpRequest(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
    // list path definitions here;
    PATH_ADD("/forward", Get);
    PATH_LIST_END
};
