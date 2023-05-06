//
// Created by wanchen.he on 2022/8/16.
//
#pragma once

#include <drogon/HttpFilter.h>
using namespace drogon;

class CoroFilter : public drogon::HttpCoroFilter<CoroFilter>
{
  public:
    Task<HttpResponsePtr> doFilter(const HttpRequestPtr &req) override;
    CoroFilter()
    {
        LOG_DEBUG << "CoroFilter constructor";
    }
};
