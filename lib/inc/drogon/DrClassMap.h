//
//  DrClassMap.h
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.
//

#ifndef DrClassMap_hpp
#define DrClassMap_hpp


#include <stdio.h>
#include <map>
#include <memory>
#include <functional>

#include <thread>
#include <mutex>

namespace drogon
{
class DrObjectBase;
typedef std::function<DrObjectBase*()>DrAllocFunc;
class DrClassMap
{
public:
    static void registerClass(const std::string &className,const DrAllocFunc &func);
    static DrObjectBase* newObject(const std::string &className);
    static std::vector<std::string> getAllClassName();
protected:
    static std::map <std::string,DrAllocFunc> &getMap();

};
}
#endif /* DrClassMap_hpp */
