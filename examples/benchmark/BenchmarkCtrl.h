#pragma once
#include <drogon/HttpSimpleController.h>
using namespace drogon;

class BenchmarkCtrl : public drogon::HttpSimpleController<BenchmarkCtrl>
{
  public:
    void asyncHandleHttpRequest(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
    PATH_ADD("/benchmark", Get);
    PATH_LIST_END
};
