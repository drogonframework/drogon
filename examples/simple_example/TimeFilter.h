//
// Created by antao on 2018/5/22.
//

#pragma once

#include <drogon/HttpFilter.h>
using namespace drogon;

class TimeFilter: public drogon::HttpFilter<TimeFilter> {
public:
    virtual std::shared_ptr<HttpResponse> doFilter(const HttpRequest& req) override;
};



