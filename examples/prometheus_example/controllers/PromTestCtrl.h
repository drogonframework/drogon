#pragma once

#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>

using namespace drogon;

class PromTestCtrl : public drogon::HttpController<PromTestCtrl>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(PromTestCtrl::fast, "/fast", "PromStat");
    ADD_METHOD_TO(PromTestCtrl::slow, "/slow", "PromStat");
    METHOD_LIST_END

    void fast(const HttpRequestPtr &req,
              std::function<void(const HttpResponsePtr &)> &&callback);
    drogon::AsyncTask slow(
        const HttpRequestPtr req,
        std::function<void(const HttpResponsePtr &)> callback);
};
