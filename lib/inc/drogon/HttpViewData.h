//
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.

#pragma once
#ifdef USE_STD_ANY

#include <any>
typedef std::any Any;
#define Any_cast std::any_cast

#elif USE_BOOST

#include <boost/any.hpp>
typedef boost::any Any;
#define Any_cast boost::any_cast

#else
#error,must use c++17 or boost
#endif


#include <map>
#include <string>

typedef std::map<std::string,Any> ViewData;
namespace drogon
{
class HttpViewData
{
public:
    template <typename T>
    const T get(const std::string &key) const
    {
        auto it=viewData_.find(key);
        if(it!=viewData_.end())
        {
            return Any_cast<T>(it->second);
        }
        T tmp;
        return tmp;
    }
    void insert(const std::string& key,Any &&obj)
    {
        viewData_[key]=std::move(obj);
    }
    void insert(const std::string& key,const Any &obj)
    {
        viewData_[key]=obj;
    }
protected:
    ViewData viewData_;
};
}
