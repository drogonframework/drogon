/**
 *
 *  PromStat.h
 *
 */

#pragma once

#include <drogon/HttpMiddleware.h>
using namespace drogon;

class PromStat : public HttpCoroMiddleware<PromStat>
{
  public:
    PromStat()
    {
    }

    Task<HttpResponsePtr> invoke(const HttpRequestPtr &req,
                                 MiddlewareNextAwaiter &&next) override;
};
