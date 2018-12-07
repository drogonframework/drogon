/**
 *
 *  PutFilter.cc
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

#include <drogon/PutFilter.h>
#include "HttpResponseImpl.h"
using namespace drogon;
void PutFilter::doFilter(const HttpRequestPtr &req,
                         const FilterCallback &fcb,
                         const FilterChainCallback &fccb)
{
    if (req->method() == Put)
    {
        fccb();
        return;
    }
    auto res = drogon::HttpResponse::newHttpResponse();
    res->setStatusCode(HttpResponse::k405MethodNotAllowed);
    fcb(res);
}
