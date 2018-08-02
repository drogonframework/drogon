//
// Created by antao on 2018/5/22.
//

#include "TimeFilter.h"
#include <trantor/utils/Date.h>
#include <trantor/utils/Logger.h>
#define VDate "visitDate"
void TimeFilter::doFilter(const HttpRequestPtr& req,
                                                   const FilterCallback &cb,
                                                   const FilterChainCallback &ccb)
{
    trantor::Date now=trantor::Date::date();
    LOG_TRACE<<"";
    if(req->session()->find(VDate))
    {
        auto lastDate=req->session()->get<trantor::Date>(VDate);
        LOG_TRACE<<"last:"<<lastDate.toFormattedString(false);
        req->session()->insert(VDate,now);
        LOG_TRACE<<"update visitDate";
        if(now>lastDate.after(10))
        {
            //10 sec later can visit again;
            ccb();
            return;
        }
        else
        {
            auto res=std::shared_ptr<HttpResponse>(drogon::HttpResponse::newHttpResponse());
            res->setBody("Visit interval should be at least 10 seconds");
            cb(res);
            return;
        }
    }
    LOG_TRACE<<"first visit,insert visitDate";
    req->session()->insert(VDate,now);
    ccb();
}