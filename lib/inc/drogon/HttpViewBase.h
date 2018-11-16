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
 *  Drogon
 *
 */

#pragma once

#include <drogon/DrObject.h>
#include <drogon/HttpViewData.h>
#include <drogon/HttpResponse.h>
#include <map>
#include <string>

namespace drogon
{
class HttpViewBase : virtual public DrObjectBase
{
  public:
    static HttpResponsePtr genHttpResponse(std::string viewName, const HttpViewData &data);

    virtual ~HttpViewBase(){};
    HttpViewBase(){};

  protected:
    virtual HttpResponsePtr genHttpResponse(const HttpViewData &) = 0;
};
} // namespace drogon
