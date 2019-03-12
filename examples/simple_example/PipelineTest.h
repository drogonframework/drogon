#pragma once
#include <drogon/HttpSimpleController.h>
using namespace drogon;
class PipelineTest : public drogon::HttpSimpleController<PipelineTest>
{
  public:
    virtual void asyncHandleHttpRequest(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback) override;
    PATH_LIST_BEGIN
    //list path definitions here;
    PATH_ADD("/pipe", Get);
    PATH_LIST_END
};
