#include "ListParaCtl.h"

void ListParaCtl::asyncHandleHttpRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // write your application logic here
    HttpViewData data;
    data.insert("title", "list parameters");
    data.insert("parameters", req->getParameters());
    auto res =
        drogon::HttpResponse::newHttpViewResponse("ListParaView.csp", data);
    callback(res);
}
