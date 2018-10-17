/**
 *
 *  DrTemplateBase.h
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
