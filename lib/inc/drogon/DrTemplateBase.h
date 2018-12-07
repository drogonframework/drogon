/**
 *
 *  DrTemplateBase.h
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

#include <drogon/DrObject.h>
#include <drogon/HttpViewData.h>
#include <string>
#include <memory>

namespace drogon
{
typedef HttpViewData DrTemplateData;
class DrTemplateBase : virtual public DrObjectBase
{
  public:
    static std::shared_ptr<DrTemplateBase> newTemplate(std::string templateName);
    virtual std::string genText(const DrTemplateData &data = DrTemplateData()) = 0;
    virtual ~DrTemplateBase(){};
    DrTemplateBase(){};
};
} // namespace drogon
