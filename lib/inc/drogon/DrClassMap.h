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

#include <drogon/utils/ClassTraits.h>
#include <trantor/utils/Logger.h>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <cxxabi.h>
#include <stdio.h>

namespace drogon
{
class DrObjectBase;
typedef std::function<DrObjectBase *()> DrAllocFunc;
class DrClassMap
{
  public:
    static void registerClass(const std::string &className,
                              const DrAllocFunc &func);
    static DrObjectBase *newObject(const std::string &className);
    static const std::shared_ptr<DrObjectBase> &getSingleInstance(
        const std::string &className);
    template <typename T>
    static std::shared_ptr<T> getSingleInstance()
    {
        static_assert(internal::IsSubClass<T, DrObjectBase>::value,
                      "T must be a sub-class of DrObjectBase");
        return std::dynamic_pointer_cast<T>(
            getSingleInstance(T::classTypeName()));
    }
    static void setSingleInstance(const std::shared_ptr<DrObjectBase> &ins);
    static std::vector<std::string> getAllClassName();
    static std::string demangle(const char *mangled_name)
    {
        std::size_t len = 0;
        int status = 0;
        std::unique_ptr<char, decltype(&std::free)> ptr(
            __cxxabiv1::__cxa_demangle(mangled_name, nullptr, &len, &status),
            &std::free);
        if (status == 0)
        {
            return std::string(ptr.get());
        }
        LOG_ERROR << "Demangle error!";
        return "";
    }

  protected:
    static std::unordered_map<std::string, DrAllocFunc> &getMap();
};

}  // namespace drogon
