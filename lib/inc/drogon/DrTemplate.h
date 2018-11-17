/**
 *
 *  DrTemplate.h
 *  An Tao
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
#include <drogon/DrTemplateBase.h>
namespace drogon
{
template <typename T>
class DrTemplate : public DrObject<T>, public DrTemplateBase
{
  protected:
    DrTemplate() {}
};
} // namespace drogon
