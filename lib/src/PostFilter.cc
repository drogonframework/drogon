//
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.
#include <drogon/PostFilter.h>
using namespace drogon;
std::shared_ptr<HttpResponse> PostFilter::doFilter(const HttpRequest& req)
{
    if(req.method()==HttpRequest::kPost)
    {
        return nullptr;
    }
    return nullptr;
}