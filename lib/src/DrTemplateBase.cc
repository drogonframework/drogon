/**
 *
 *  DrTemplateBase.cc
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

#include <drogon/DrClassMap.h>
#include <drogon/DrTemplateBase.h>
#include <memory>
#include <trantor/utils/Logger.h>

using namespace drogon;

std::shared_ptr<DrTemplateBase> DrTemplateBase::newTemplate(
    std::string templateName)
{
    LOG_TRACE << "http view name=" << templateName;
    auto pos = templateName.find(".csp");
    if (pos != std::string::npos)
    {
        if (pos == templateName.length() - 4)
        {
            templateName = templateName.substr(0, pos);
        }
    }
    return std::shared_ptr<DrTemplateBase>(dynamic_cast<DrTemplateBase*>(
        drogon::DrClassMap::newObject(templateName)));
}
