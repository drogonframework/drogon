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
        : _field(field), _value(value)
    {
    }
    virtual void doFilter(const HttpRequestPtr &req,
                          FilterCallback &&fcb,
                          FilterChainCallback &&fccb) override;

  private:
    std::string _field;
    std::string _value;
};
