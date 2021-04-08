/**
 *
 *  @file DrObject.h
 *  @author An Tao
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

#include <drogon/exports.h>
#include <drogon/DrClassMap.h>

#include <string>
#include <type_traits>

#ifdef _MSC_VER
#pragma warning(disable : 4250)
#endif

namespace drogon
{
/**
 * @brief The base class for all drogon reflection classes.
 *
 */
class DROGON_EXPORT DrObjectBase
{
  public:
    /**
     * @brief Get the class name
     *
     * @return const std::string& the class name
     */
    virtual const std::string &className() const
    {
        static const std::string name{"DrObjectBase"};
        return name;
    }

    /**
     * @brief Return true if the class name is 'class_name'
     */
    virtual bool isClass(const std::string &class_name) const
    {
        return (className() == class_name);
    }
    virtual ~DrObjectBase()
    {
    }
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
        return alloc_.className();
    }
    static const std::string &classTypeName()
    {
        return alloc_.className();
    }

    virtual bool isClass(const std::string &class_name) const override
    {
        return (className() == class_name);
    }

  protected:
    // protect constructor to make this class only inheritable
    DrObject() = default;
    ~DrObject() override = default;

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
            static std::string className =
                DrClassMap::demangle(typeid(T).name());
            return className;
        }
        template <typename D>
        typename std::enable_if<std::is_default_constructible<D>::value,
                                void>::type
        registerClass()
        {
            DrClassMap::registerClass(className(),
                                      []() -> DrObjectBase * { return new T; });
        }
        template <typename D>
        typename std::enable_if<!std::is_default_constructible<D>::value,
                                void>::type
        registerClass()
        {
        }
    };

    // use static val to register allocator function for class T;
    static DrAllocator alloc_;
};
template <typename T>
typename DrObject<T>::DrAllocator DrObject<T>::alloc_;

}  // namespace drogon
