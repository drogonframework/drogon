//
// Created by antao on 2018/5/22.
//

#pragma once

#include <drogon/HttpFilter.h>
using namespace drogon;

class TimeFilter : public drogon::HttpFilter<TimeFilter>
{
  public:
    void doFilter(const HttpRequestPtr &req,
                  FilterCallback &&cb,
                  FilterChainCallback &&ccb) override;

    TimeFilter()
    {
        LOG_DEBUG << "TimeFilter constructor";
    }
};
