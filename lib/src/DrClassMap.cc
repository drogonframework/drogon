/**
 *
 *  DrClassMap.cc
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
#include <drogon/DrObject.h>
#include <trantor/utils/Logger.h>

using namespace drogon;

namespace drogon
{
namespace internal
{
static std::unordered_map<std::string, std::shared_ptr<DrObjectBase>> &
getObjsMap()
{
    static std::unordered_map<std::string, std::shared_ptr<DrObjectBase>>
        singleInstanceMap;
    return singleInstanceMap;
}

static std::mutex &getMapMutex()
{
    static std::mutex mtx;
    return mtx;
}

}  // namespace internal
}  // namespace drogon

void DrClassMap::registerClass(const std::string &className,
                               const DrAllocFunc &func,
                               const DrSharedAllocFunc &sharedFunc)
{
    LOG_TRACE << "Register class:" << className;
    getMap().insert(
        std::make_pair(className, std::make_pair(func, sharedFunc)));
}

DrObjectBase *DrClassMap::newObject(const std::string &className)
{
    auto iter = getMap().find(className);
    if (iter != getMap().end())
    {
        return iter->second.first();
    }
    else
        return nullptr;
}

std::shared_ptr<DrObjectBase> DrClassMap::newSharedObject(
    const std::string &className)
{
    auto iter = getMap().find(className);
    if (iter != getMap().end())
    {
        if (iter->second.second)
            return iter->second.second();
        else
            return std::shared_ptr<DrObjectBase>(iter->second.first());
    }
    else
        return nullptr;
}

const std::shared_ptr<DrObjectBase> &DrClassMap::getSingleInstance(
    const std::string &className)
{
    auto &mtx = internal::getMapMutex();
    auto &singleInstanceMap = internal::getObjsMap();
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto iter = singleInstanceMap.find(className);
        if (iter != singleInstanceMap.end())
            return iter->second;
    }
    auto newObj = newSharedObject(className);
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto ret = singleInstanceMap.insert(
            std::make_pair(className, std::move(newObj)));
        return ret.first->second;
    }
}

void DrClassMap::setSingleInstance(const std::shared_ptr<DrObjectBase> &ins)
{
    auto &mtx = internal::getMapMutex();
    auto &singleInstanceMap = internal::getObjsMap();
    std::lock_guard<std::mutex> lock(mtx);
    singleInstanceMap[ins->className()] = ins;
}

std::vector<std::string> DrClassMap::getAllClassName()
{
    std::vector<std::string> ret;
    for (auto const &iter : getMap())
    {
        ret.push_back(iter.first);
    }
    return ret;
}

std::unordered_map<std::string, std::pair<DrAllocFunc, DrSharedAllocFunc>> &
DrClassMap::getMap()
{
    static std::unordered_map<std::string,
                              std::pair<DrAllocFunc, DrSharedAllocFunc>>
        map;
    return map;
}
