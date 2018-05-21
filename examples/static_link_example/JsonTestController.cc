#include "JsonTestController.h"
#include <json/json.h>
void JsonTestController::asyncHandleHttpRequest(const HttpRequest& req,std::function<void (HttpResponse &)>callback)
{
    Json::Value json;
    json["path"]="json";
    json["name"]="json test";
    Json::Value array;
    for(int i=0;i<5;i++)
    {
        Json::Value user;
        user["id"]=i;
        user["name"]="none";
        array.append(user);
    }
    json["rows"]=array;
    auto resp=std::unique_ptr<HttpResponse>(HttpResponse::newHttpResponse(json));
    callback(*resp);
}