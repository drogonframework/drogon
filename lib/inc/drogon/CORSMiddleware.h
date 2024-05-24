/**
 *
 *  @file CORSMiddleware.h
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once
#include <drogon/HttpMiddleware.h>

namespace drogon
{
/**
 * @brief This class represents a middleware that adds CORS headers to the
 * response.
 */
class CORSMiddleware : public HttpMiddleware<CORSMiddleware>
{
  public:
    CORSMiddleware()
    {
    }

    void invoke(const HttpRequestPtr &req,
                MiddlewareNextCallback &&nextCb,
                MiddlewareCallback &&mcb) override;

    void setAllowOrigins(const std::vector<std::string> &origins)
    {
        allowOrigins_ = origins;
    }

    void setAllowHeaders(const std::vector<std::string> &headers)
    {
        allowHeaders_ = headers;
    }

    void setAllowCredentials(bool allowCredentials)
    {
        allowCredentials_ = allowCredentials;
    }

    void setExposeHeaders(const std::vector<std::string> &headers)
    {
        exposeHeaders_ = headers;
    }

    void setOriginRegex(const std::string &regex)
    {
        originRegex_ = regex;
    }

    void setMaxAge(int maxAge)
    {
        maxAge_ = maxAge;
    }

  private:
    std::vector<std::string> allowOrigins_;
    std::vector<std::string> allowHeaders_;
    std::vector<std::string> exposeHeaders_;
    bool allowCredentials_{false};
    std::string originRegex_;
    int maxAge_{0};
};
}  // namespace drogon
