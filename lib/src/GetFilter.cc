//
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.
#include <drogon/GetFilter.h>
#include "HttpResponseImpl.h"
using namespace drogon;
std::shared_ptr<HttpResponse> GetFilter::doFilter(const HttpRequest& req)
{
    if(req.method()==HttpRequest::kGet)
    {
        return nullptr;
    }
    auto res=std::shared_ptr<HttpResponse>(HttpResponse::newHttpResponse());
    if(res)
    {
        res->setStatusCode(HttpResponse::k404NotFound);
        res->setCloseConnection(true);
    }

    return res;
}