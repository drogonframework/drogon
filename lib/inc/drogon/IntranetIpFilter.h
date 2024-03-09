/**
 *
 *  @file IntranetIpFilter.h
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

#include <drogon/exports.h>
#include <drogon/HttpFilter.h>

namespace drogon
{
/**
 * @brief A filter that prohibit access from external networks
 */
class DROGON_EXPORT IntranetIpFilter : public HttpFilter<IntranetIpFilter>
{
  public:
    IntranetIpFilter()
    {
    }

    void doFilter(const HttpRequestPtr &req,
                  FilterCallback &&fcb,
                  FilterChainCallback &&fccb) override;
};
}  // namespace drogon
