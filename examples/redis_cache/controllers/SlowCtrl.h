#pragma once
#include <drogon/HttpController.h>
using namespace drogon;
class SlowCtrl : public drogon::HttpController<SlowCtrl>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SlowCtrl::hello, "/slow?userid={userid}", Get, "TimeFilter");
    ADD_METHOD_TO(SlowCtrl::observe, "/observe?userid={userid}", Get);
    METHOD_LIST_END

    void hello(const HttpRequestPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback,
               std::string &&userid);
    drogon::AsyncTask observe(
        HttpRequestPtr req,
        std::function<void(const HttpResponsePtr &)> callback,
        std::string userid);
};
