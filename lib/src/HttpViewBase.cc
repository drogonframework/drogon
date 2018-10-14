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
#include <drogon/HttpViewBase.h>
#include <drogon/DrClassMap.h>
#include <trantor/utils/Logger.h>
#include <memory>
using namespace drogon;
HttpResponsePtr HttpViewBase::genHttpResponse(std::string viewName, const HttpViewData &data)
{
    LOG_TRACE << "http view name=" << viewName;
    auto pos = viewName.find(".csp");
    if (pos != std::string::npos)
    {
        if (pos == viewName.length() - 4)
        {
            viewName = viewName.substr(0, pos);
        }
    }
    auto obj = std::shared_ptr<DrObjectBase>(drogon::DrClassMap::newObject(viewName));
    if (obj)
    {
        auto view = std::dynamic_pointer_cast<HttpViewBase>(obj);
        if (view)
        {
            return view->genHttpResponse(data);
        }
    }
    return drogon::HttpResponse::newNotFoundResponse();
}
