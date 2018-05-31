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
#include <drogon/HttpViewBase.h>
namespace drogon {
    template <typename T>
    class HttpView:public DrObject<T>,public HttpViewBase
    {
    protected:
        HttpView(){}
    };
}
