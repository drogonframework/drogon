#pragma once
#include <drogon/HttpSimpleController.h>
using namespace drogon;
class ListParaCtl : public drogon::HttpSimpleController<ListParaCtl>
{
  public:
    virtual void asyncHandleHttpRequest(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
    // list path definations here;
    // PATH_ADD("/path","filter1","filter2",...);
    PATH_ADD("/listpara", Get);
    PATH_LIST_END
};
