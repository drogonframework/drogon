/**
 *
 *  DrObject.h
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

#include <drogon/DrClassMap.h>

#include <string>
#include <type_traits>

namespace drogon
{
class DrObjectBase
{
  public:
    virtual const std::string &className() const
    {
        static std::string _name = "DrObjectBase";
        return _name;
    }
    virtual bool isClass(const std::string &class_name) const
    {
        return (className() == class_name);
    }
    virtual ~DrObjectBase() {}
};

/**
 * a class template to
 * implement the reflection function of creating the class object by class name
 */
template <typename T>
class DrObject : public virtual DrObjectBase
{
  public:
    virtual const std::string &className() const override
    {
        return _alloc.className();
    }
    static const std::string &classTypeName()
    {
        return _alloc.className();
    }

    virtual bool isClass(const std::string &class_name) const override
    {
        return (className() == class_name);
    }

  protected:
    //protect constructor to make this class only inheritable
    DrObject() {}

  private:
    class DrAllocator
    {
      public:
        DrAllocator()
        {
            registerClass<T>();
        }
        const std::string &className() const
        {
            static std::string className = DrClassMap::demangle(typeid(T).name());
            return className;
        }
        template <typename D>
        typename std::enable_if<std::is_default_constructible<D>::value, void>::type registerClass()
        {
            DrClassMap::registerClass(className(), []() -> DrObjectBase * {
                return new T;
            });
        }
        template <typename D>
        typename std::enable_if<!std::is_default_constructible<D>::value, void>::type registerClass()
        {
        }
    };

    //use static val to register allocator function for class T;
    static DrAllocator _alloc;
};
template <typename T>
typename DrObject<T>::DrAllocator DrObject<T>::_alloc;

} // namespace drogon
