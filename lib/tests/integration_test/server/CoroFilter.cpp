//
// Created by wanchen.he on 2022/8/16.
//

#include "CoroFilter.h"

Task<HttpResponsePtr> CoroFilter::doFilter(const HttpRequestPtr& req)
{
    int secs = std::stoi(req->getParameter("secs"));
    co_await sleepCoro(trantor::EventLoop::getEventLoopOfCurrentThread(), secs);
    co_return {};
}
