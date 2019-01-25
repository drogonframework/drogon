/**
 *
 *  DrClassMap.h
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

#include <stdio.h>
#include <unordered_map>
#include <memory>
#include <functional>

#include <thread>
#include <mutex>
#include <vector>

namespace drogon
{
class DrObjectBase;
typedef std::function<DrObjectBase *()> DrAllocFunc;
class DrClassMap
{
  public:
    static void registerClass(const std::string &className, const DrAllocFunc &func);
    static DrObjectBase *newObject(const std::string &className);
    static const std::shared_ptr<DrObjectBase> &getSingleInstance(const std::string &className);
    static std::vector<std::string> getAllClassName();

  protected:
    static std::unordered_map<std::string, DrAllocFunc> &getMap();
};
} // namespace drogon
