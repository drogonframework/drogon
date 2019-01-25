/**
 *
 *  FiltersFunction.h
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

#pragma once

#include "HttpRequestImpl.h"
#include <string>
#include <vector>
namespace drogon
{

struct FiltersFunction
{
    static void doFilters(const std::vector<std::string> &filters,
                          const HttpRequestImplPtr &req,
                          const std::shared_ptr<const std::function<void(const HttpResponsePtr &)>> &callbackPtr,
                          bool needSetJsessionid,
                          const std::shared_ptr<std::string> &sessionIdPtr,
                          std::function<void()> &&missCallback);
};

} // namespace drogon