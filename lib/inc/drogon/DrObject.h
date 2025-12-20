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

template <typename T>
struct isAutoCreationClass
{
    template <class C>
    static constexpr auto check(C *) -> std::enable_if_t<
        std::is_same_v<decltype(C::isAutoCreation), const bool>,
        bool>
    {
        return C::isAutoCreation;
    }

    template <typename>
    static constexpr bool check(...)
    {
        return false;
    }

    static constexpr bool value = check<T>(nullptr);
};

/**
 * a class template to
 * implement the reflection function of creating the class object by class name
 */
template <typename T>
class DrObject : public virtual DrObjectBase
{
  public:
    const std::string &className() const override
    {
        return alloc_.className();
    }

    static const std::string &classTypeName()
    {
        return alloc_.className();
    }

    bool isClass(const std::string &class_name) const override
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
        void registerClass()
        {
            if constexpr (std::is_default_constructible<D>::value)
            {
                DrClassMap::registerClass(
                    className(),
                    []() -> DrObjectBase * { return new T; },
                    []() -> std::shared_ptr<DrObjectBase> {
                        return std::make_shared<T>();
                    });
            }
            else if constexpr (isAutoCreationClass<D>::value)
            {
                static_assert(std::is_default_constructible<D>::value,
                              "Class is not default constructable!");
            }
        }
    };

    // use static val to register allocator function for class T;
    static DrAllocator alloc_;
};

template <typename T>
typename DrObject<T>::DrAllocator DrObject<T>::alloc_;

}  // namespace drogon
