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

#pragma once

#include <drogon/DrObject.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpRequest.h>
#include <memory>

namespace drogon
{
    class HttpFilterBase:public virtual DrObjectBase
    {
    public:
        virtual std::shared_ptr<HttpResponse> doFilter(const HttpRequest& req)=0;
        virtual ~HttpFilterBase(){}
    };
    template <typename T>
    class HttpFilter:public DrObject<T>,public HttpFilterBase
    {
    public:
        virtual ~HttpFilter(){}
    };
}