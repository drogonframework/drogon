#include "SlowCtrl.h"
#include "RedisCache.h"
#include "DateFuncs.h"
#include <drogon/drogon.h>
#define VDate "visitDate"
void SlowCtrl::hello(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     std::string &&userid)
{
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody("hello, " + userid);
    callback(resp);
}

drogon::AsyncTask SlowCtrl::observe(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback,
    std::string userid)
{
    auto key = userid + "." + VDate;
    auto redisPtr = drogon::app().getFastRedisClient();
    try
    {
        auto date = co_await getFromCache<trantor::Date>(key, redisPtr);
        auto resp = drogon::HttpResponse::newHttpResponse();
        std::string body{"your last visit time: "};
        body.append(date.toFormattedStringLocal(false));
        resp->setBody(std::move(body));
        callback(resp);
    }
    catch (const std::exception &err)
    {
        LOG_ERROR << err.what();
        callback(drogon::HttpResponse::newNotFoundResponse());
    }
}
