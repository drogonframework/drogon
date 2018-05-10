//
//  DrClassMap.cc
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.
//

#include <drogon/DrClassMap.h>
#include <iostream>
using namespace drogon;
std::map <std::string,DrAllocFunc> * DrClassMap::classMap=nullptr;
std::once_flag DrClassMap::flag;
void DrClassMap::registerClass(const std::string &className,const DrAllocFunc &func)
{
    std::call_once(flag, [](){classMap=new std::map<std::string,DrAllocFunc>;});
    std::cout<<"register class:"<<className<<std::endl;
    classMap->insert(std::make_pair(className, func));
}
DrObjectBase* DrClassMap::newObject(const std::string &className)
{
    std::call_once(flag, [](){classMap=new std::map<std::string,DrAllocFunc>;});
    auto iter=classMap->find(className);
    if(iter!=classMap->end())
    {
        return iter->second();
    }
    else
        return nullptr;
}