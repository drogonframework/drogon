/**
 *
 *  @file DrClassMap.h
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
#include <trantor/utils/Logger.h>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <type_traits>
#include <cstdlib>
#ifndef _MSC_VER
#include <cxxabi.h>
#endif
#include <stdio.h>

namespace drogon
{
class DrObjectBase;
using DrAllocFunc = std::function<DrObjectBase *()>;

/**
 * @brief A map class which can create DrObjects from names.
 */
class DROGON_EXPORT DrClassMap
{
  public:
    /**
     * @brief Register a class into the map
     *
     * @param className The name of the class
     * @param func The function which can create a new instance of the class.
     */
    static void registerClass(const std::string &className,
                              const DrAllocFunc &func);

    /**
     * @brief Create a new instance of the class named by className
     *
     * @param className The name of the class
     * @return DrObjectBase* The pointer to the newly created instance.
     */
    static DrObjectBase *newObject(const std::string &className);

    /**
     * @brief Get the singleton object of the class named by className
     *
     * @param className The name of the class
     * @return const std::shared_ptr<DrObjectBase>& The smart pointer to the
     * instance.
     */
    static const std::shared_ptr<DrObjectBase> &getSingleInstance(
        const std::string &className);

    /**
     * @brief Get the singleton T type object
     *
     * @tparam T The type of the class
     * @return std::shared_ptr<T> The smart pointer to the instance.
     * @note The T must be a subclass of the DrObjectBase class.
     */
    template <typename T>
    static std::shared_ptr<T> getSingleInstance()
    {
        static_assert(std::is_base_of<DrObjectBase, T>::value,
                      "T must be a sub-class of DrObjectBase");
        static auto const singleton =
            std::dynamic_pointer_cast<T>(getSingleInstance(T::classTypeName()));
        assert(singleton);
        return singleton;
    }

    /**
     * @brief Set a singleton object into the map.
     *
     * @param ins The smart pointer to the instance.
     */
    static void setSingleInstance(const std::shared_ptr<DrObjectBase> &ins);

    /**
     * @brief Get all names of classes registered in the map.
     *
     * @return std::vector<std::string> the vector of class names.
     */
    static std::vector<std::string> getAllClassName();

    /**
     * @brief demangle the type name which is returned by typeid(T).name().
     *
     * @param mangled_name The type name which is returned by typeid(T).name().
     * @return std::string The human readable type name.
     */
    static std::string demangle(const char *mangled_name)
    {
#ifndef _MSC_VER
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
#else
        auto pos = strstr(mangled_name, " ");
        if (pos == nullptr)
            return std::string{mangled_name};
        else
            return std::string{pos + 1};
#endif
    }

  protected:
    static std::unordered_map<std::string, DrAllocFunc> &getMap();
};

}  // namespace drogon
