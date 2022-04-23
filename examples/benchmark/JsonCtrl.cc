#include "JsonCtrl.h"
void JsonCtrl::asyncHandleHttpRequest(
    const HttpRequestPtr &,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    Json::Value ret;
    ret["message"] = "Hello, World!";
    auto resp = HttpResponse::newHttpJsonResponse(ret);
    callback(resp);
}