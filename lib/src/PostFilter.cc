/**
 *
 *  PostFilter.cc
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

#include <drogon/PostFilter.h>
#include "HttpResponseImpl.h"
using namespace drogon;
void PostFilter::doFilter(const HttpRequestPtr &req,
                          const FilterCallback &fcb,
                          const FilterChainCallback &fccb)
{
    if (req->method() == Post)
    {
        fccb();
        return;
    }
    auto res = drogon::HttpResponse::newHttpResponse();
    res->setStatusCode(HttpResponse::k405MethodNotAllowed);
    fcb(res);
}
