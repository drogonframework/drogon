/**
 *
 *  CustomHeaderFilter.h
 *
 */

#pragma once

#include <drogon/HttpFilter.h>
using namespace drogon;

class CustomHeaderFilter : public HttpFilter<CustomHeaderFilter, false>
{
  public:
    CustomHeaderFilter(const std::string &field, const std::string &value)
        : field_(field), value_(value)
    {
    }

    void doFilter(const HttpRequestPtr &req,
                  FilterCallback &&fcb,
                  FilterChainCallback &&fccb) override;

  private:
    std::string field_;
    std::string value_;
};
