//
// Created by antao on 2018/5/22.
//

#include "TimeFilter.h"
#include <drogon/utils/coroutine.h>
#include <drogon/drogon.h>
#include <string>
#include "RedisCache.h"

#define VDate "visitDate"

template <>
trantor::Date fromString<trantor::Date>(const std::string &str)
{
    return trantor::Date(std::atoll(str.data()));
}
template <>
std::string toString<trantor::Date>(const trantor::Date &date)
{
    return std::to_string(date.microSecondsSinceEpoch());
}
void TimeFilter::doFilter(const HttpRequestPtr &req,
                          FilterCallback &&cb,
                          FilterChainCallback &&ccb)
{
    drogon::async_run(
        [req, cb = std::move(cb), ccb = std::move(ccb)]() -> drogon::Task<> {
            auto userid = req->getParameter("userid");
            auto key = userid + "." + VDate;
            if (!userid.empty())
            {
                const trantor::Date now = trantor::Date::date();
                auto redisClient = drogon::app().getFastRedisClient();
                try
                {
                    auto lastDate =
                        co_await getFromCache<trantor::Date>(key, redisClient);
                    LOG_TRACE << "last:" << lastDate.toFormattedString(false);
                    co_await updateCache(key, now, redisClient);
                    if (now > lastDate.after(10))
                    {
                        // 10 sec later can visit again;
                        ccb();
                        co_return;
                    }
                    else
                    {
                        Json::Value json;
                        json["result"] = "error";
                        json["message"] =
                            "Access interval should be at least 10 seconds";
                        auto res = HttpResponse::newHttpJsonResponse(json);
                        cb(res);
                        co_return;
                    }
                }
                catch (const std::exception &err)
                {
                    LOG_TRACE << "first visit,insert visitDate";
                    co_await updateCache(userid + "." VDate, now, redisClient);
                    ccb();
                    co_return;
                }
            }
            else
            {
                auto resp = HttpResponse::newNotFoundResponse();
                cb(resp);
                co_return;
            }
        });
}