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

#include <drogon/DeleteFilter.h>
#include "HttpResponseImpl.h"
using namespace drogon;
std::shared_ptr<HttpResponse> DeleteFilter::doFilter(const HttpRequest& req)
{
    if(req.method()==HttpRequest::kDelete)
    {
        return nullptr;
    }
    auto res=drogon::HttpResponse::notFoundResponse();


    return res;
}