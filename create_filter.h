/**
 *
 *  create_filter.h
 *  drogon_ctl
 *
 */

#pragma once

#include <drogon/HttpFilter.h>
using namespace drogon;

class create_filter : public HttpFilter<create_filter>
{
  public:
    create_filter() {}
    virtual void doFilter(const HttpRequestPtr &req,
                          const FilterCallback &fcb,
                          const FilterChainCallback &fccb) override;
};
