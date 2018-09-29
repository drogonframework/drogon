//
// Created by antao on 2018/5/22.
//

#include "TimeFilter.h"
#define VDate "visitDate"
void TimeFilter::doFilter(const HttpRequestPtr& req,
                                                   const FilterCallback &cb,
                                                   const FilterChainCallback &ccb)
{
    trantor::Date now=trantor::Date::date();
    if(!req->session())
    {
        //no session support by framework,pls enable session
        auto resp=HttpResponse::newNotFoundResponse();
        cb(resp);
        return;
    }
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
            Json::Value json;
            json["result"]="error";
            json["message"]="Visit interval should be at least 10 seconds";
            auto res=HttpResponse::newHttpJsonResponse(json);
            cb(res);
            return;
        }
    }
    LOG_TRACE<<"first visit,insert visitDate";
    req->session()->insert(VDate,now);
    ccb();
}