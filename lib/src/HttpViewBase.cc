/**
 *
 *  HttpViewBase.h
 *  An Tao
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

#include <drogon/DrTemplateBase.h>
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

    auto templ = DrTemplateBase::newTemplate(viewName);
    if (templ)
    {
        auto res = HttpResponse::newHttpResponse();
        res->setStatusCode(HttpResponse::k200OK);
        res->setContentTypeCode(CT_TEXT_HTML);
        res->setBody(templ->genText(data));
        return res;
    }

    return drogon::HttpResponse::newNotFoundResponse();
}
