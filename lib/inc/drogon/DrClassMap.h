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

/**
 * @brief A map class which can create DrObjects from names.
 */
class DrClassMap
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
        static_assert(internal::IsSubClass<T, DrObjectBase>::value,
                      "T must be a sub-class of DrObjectBase");
        return std::dynamic_pointer_cast<T>(
            getSingleInstance(T::classTypeName()));
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
