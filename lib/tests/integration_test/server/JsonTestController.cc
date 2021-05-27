#include "JsonTestController.h"
#include <json/json.h>
void JsonTestController::asyncHandleHttpRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    Json::Value json;
    json["path"] = "json";
    json["name"] = "json test";
    Json::Value array;
    for (int i = 0; i < 5; ++i)
    {
        Json::Value user;
        user["id"] = i;
        user["name"] = "none";
        user["c_name"] = "张三";
        array.append(user);
    }
    json["rows"] = array;
    auto resp = HttpResponse::newHttpJsonResponse(json);
    assert(resp->jsonObject().get());
    callback(resp);
}
