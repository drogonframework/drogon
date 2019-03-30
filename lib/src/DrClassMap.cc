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
#include <iostream>
using namespace drogon;
//std::map <std::string,DrAllocFunc> * DrClassMap::classMap=nullptr;
//std::once_flag DrClassMap::flag;
namespace drogon
{
namespace internal
{

static std::unordered_map<std::string, std::shared_ptr<DrObjectBase>> &getObjsMap()
{
    static std::unordered_map<std::string, std::shared_ptr<DrObjectBase>> singleInstanceMap;
    return singleInstanceMap;
}

static std::mutex &getMapMutex()
{
    static std::mutex mtx;
    return mtx;
}

} // namespace internal
} // namespace drogon

void DrClassMap::registerClass(const std::string &className, const DrAllocFunc &func)
{
    getMap().insert(std::make_pair(className, func));
}

DrObjectBase *DrClassMap::newObject(const std::string &className)
{
    auto iter = getMap().find(className);
    if (iter != getMap().end())
    {
        return iter->second();
    }
    else
        return nullptr;
}

const std::shared_ptr<DrObjectBase> &DrClassMap::getSingleInstance(const std::string &className)
{
    auto &mtx = internal::getMapMutex();
    auto &singleInstanceMap = internal::getObjsMap();
    std::lock_guard<std::mutex> lock(mtx);
    auto iter = singleInstanceMap.find(className);
    if (iter != singleInstanceMap.end())
        return iter->second;
    singleInstanceMap[className] = std::shared_ptr<DrObjectBase>(newObject(className));
    return singleInstanceMap[className];
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

std::unordered_map<std::string, DrAllocFunc> &DrClassMap::getMap()
{
    static std::unordered_map<std::string, DrAllocFunc> map;
    return map;
}
