/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#include <drogon/GetFilter.h>
#include "HttpResponseImpl.h"
using namespace drogon;
void GetFilter::doFilter(const HttpRequestPtr &req,
                         const FilterCallback &fcb,
                         const FilterChainCallback &fccb)
{
    if (req->method() == Get)
    {
        fccb();
        return;
    }
    auto res = drogon::HttpResponse::newHttpResponse();
    res->setStatusCode(HttpResponse::k405MethodNotAllowed);
    fcb(res);
}
