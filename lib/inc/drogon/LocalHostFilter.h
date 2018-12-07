/**
 *
 *  LocalHostFilter.h
 *  An Tao
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

#include <drogon/HttpFilter.h>

namespace drogon
{
class LocalHostFilter : public HttpFilter<LocalHostFilter>
{
  public:
    LocalHostFilter() {}
    virtual void doFilter(const HttpRequestPtr &req,
                          const FilterCallback &fcb,
                          const FilterChainCallback &fccb) override;
};
} // namespace drogon
