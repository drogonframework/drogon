/**
 *
 *  @file DrTemplateBase.cc
 *  @author An Tao
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
#include <trantor/utils/Logger.h>
#include <memory>
#include <regex>

using namespace drogon;

std::shared_ptr<DrTemplateBase> DrTemplateBase::newTemplate(
    const std::string &templateName)
{
    LOG_TRACE << "http view name=" << templateName;
    auto l = templateName.length();
    if (l >= 4 && templateName[l - 4] == '.' && templateName[l - 3] == 'c' &&
        templateName[l - 2] == 's' && templateName[l - 1] == 'p')
    {
        std::string::size_type pos = 0;
        std::string newName;
        newName.reserve(templateName.size());
        if (templateName[0] == '/' || templateName[0] == '\\')
        {
            pos = 1;
        }
        else if (templateName[0] == '.' &&
                 (templateName[1] == '/' || templateName[1] == '\\'))
        {
            pos = 2;
        }
        while (pos < l - 4)
        {
            if (templateName[pos] == '/' || templateName[pos] == '\\')
            {
                newName.append("::");
            }
            else
            {
                newName.append(1, templateName[pos]);
            }
            ++pos;
        }
        return std::shared_ptr<DrTemplateBase>(dynamic_cast<DrTemplateBase *>(
            drogon::DrClassMap::newObject(newName)));
    }
    else
    {
        return std::shared_ptr<DrTemplateBase>(dynamic_cast<DrTemplateBase *>(
            drogon::DrClassMap::newObject(templateName)));
    }
}
