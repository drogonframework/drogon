/**
 *
 *  @file HttpBinder.h
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

/// The classes in the file are internal tool classes. Do not include this
/// file directly and use any of these classes directly.

#pragma once

#include <drogon/exports.h>
#include <drogon/DrClassMap.h>
#include <drogon/DrObject.h>
#include <drogon/utils/FunctionTraits.h>
#include <drogon/utils/Utilities.h>
#include <drogon/HttpRequest.h>
#include <deque>
#include <memory>
#include <sstream>
#include <string>

namespace drogon
{
namespace internal
{
// we only accept value type or const lreference type or right reference type as
// the handle method parameters type
template <typename T>
struct BinderArgTypeTraits
{
    static const bool isValid = true;
};
template <typename T>
struct BinderArgTypeTraits<T *>
{
    static const bool isValid = false;
};
template <typename T>
struct BinderArgTypeTraits<T &>
{
    static const bool isValid = false;
};
template <typename T>
struct BinderArgTypeTraits<T &&>
{
    static const bool isValid = true;
};
template <typename T>
struct BinderArgTypeTraits<const T &&>
{
    static const bool isValid = false;
};
template <typename T>
struct BinderArgTypeTraits<const T &>
{
    static const bool isValid = true;
};

class HttpBinderBase
{
  public:
    virtual void handleHttpRequest(
        std::deque<std::string> &pathArguments,
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) = 0;
    virtual size_t paramCount() = 0;
    virtual const std::string &handlerName() const = 0;
    virtual ~HttpBinderBase()
    {
    }
};

template <typename T>
T &getControllerObj()
{
    // Initialization of function-local statics is guaranteed to occur only once
    // even when
    // called from multiple threads, and may be more efficient than the
    // equivalent code using std::call_once.
    static T obj;
    return obj;
}

DROGON_EXPORT void handleException(
    const std::exception &,
    const HttpRequestPtr &,
    std::function<void(const HttpResponsePtr &)> &&);

using HttpBinderBasePtr = std::shared_ptr<HttpBinderBase>;
template <typename FUNCTION>
class HttpBinder : public HttpBinderBase
{
  public:
    using traits = FunctionTraits<FUNCTION>;
    using FunctionType = FUNCTION;
    void handleHttpRequest(
        std::deque<std::string> &pathArguments,
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) override
    {
        run(pathArguments, req, std::move(callback));
    }
    size_t paramCount() override
    {
        return traits::arity;
    }
    HttpBinder(FUNCTION &&func) : func_(std::forward<FUNCTION>(func))
    {
        static_assert(traits::isHTTPFunction,
                      "Your API handler function interface is wrong!");
        handlerName_ = DrClassMap::demangle(typeid(FUNCTION).name());
    }
    void test()
    {
        std::cout << "argument_count=" << argument_count << " "
                  << traits::isHTTPFunction << std::endl;
    }
    const std::string &handlerName() const override
    {
        return handlerName_;
    }
    template <bool isClassFunction = traits::isClassFunction,
              bool isDrObjectClass = traits::isDrObjectClass>
    typename std::enable_if<isDrObjectClass && isClassFunction, void>::type
    createHandlerInstance()
    {
        auto objPtr =
            DrClassMap::getSingleInstance<typename traits::class_type>();
        LOG_TRACE << "create handler class object: " << objPtr.get();
    }
    template <bool isClassFunction = traits::isClassFunction,
              bool isDrObjectClass = traits::isDrObjectClass>
    typename std::enable_if<!isDrObjectClass && isClassFunction, void>::type
    createHandlerInstance()
    {
        auto &obj = getControllerObj<typename traits::class_type>();
        LOG_TRACE << "create handler class object: " << &obj;
    }
    template <bool isClassFunction = traits::isClassFunction>
    typename std::enable_if<!isClassFunction, void>::type
    createHandlerInstance()
    {
    }

  private:
    FUNCTION func_;
    template <std::size_t Index>
    using nth_argument_type = typename traits::template argument<Index>;

    static const size_t argument_count = traits::arity;
    std::string handlerName_;

    template <typename T>
    typename std::enable_if<internal::CanConvertFromStringStream<T>::value,
                            void>::type
    getHandlerArgumentValue(T &value, std::string &&p)
    {
        if (!p.empty())
        {
            std::stringstream ss(std::move(p));
            ss >> value;
        }
    }

    template <typename T>
    typename std::enable_if<!(internal::CanConvertFromStringStream<T>::value),
                            void>::type
    getHandlerArgumentValue(T &value, std::string &&p)
    {
    }

    void getHandlerArgumentValue(std::string &value, std::string &&p)
    {
        value = std::move(p);
    }

    void getHandlerArgumentValue(int &value, std::string &&p)
    {
        value = std::stoi(p);
    }

    void getHandlerArgumentValue(long &value, std::string &&p)
    {
        value = std::stol(p);
    }

    void getHandlerArgumentValue(long long &value, std::string &&p)
    {
        value = std::stoll(p);
    }

    void getHandlerArgumentValue(unsigned long &value, std::string &&p)
    {
        value = std::stoul(p);
    }

    void getHandlerArgumentValue(unsigned long long &value, std::string &&p)
    {
        value = std::stoull(p);
    }

    void getHandlerArgumentValue(float &value, std::string &&p)
    {
        value = std::stof(p);
    }

    void getHandlerArgumentValue(double &value, std::string &&p)
    {
        value = std::stod(p);
    }

    void getHandlerArgumentValue(long double &value, std::string &&p)
    {
        value = std::stold(p);
    }

    template <typename... Values, std::size_t Boundary = argument_count>
    typename std::enable_if<(sizeof...(Values) < Boundary), void>::type run(
        std::deque<std::string> &pathArguments,
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        Values &&...values)
    {
        // Call this function recursively until parameter's count equals to the
        // count of target function parameters
        static_assert(
            BinderArgTypeTraits<nth_argument_type<sizeof...(Values)>>::isValid,
            "your handler argument type must be value type or const left "
            "reference type or right reference type");
        using ValueType =
            typename std::remove_cv<typename std::remove_reference<
                nth_argument_type<sizeof...(Values)>>::type>::type;
        ValueType value = ValueType();
        if (!pathArguments.empty())
        {
            std::string v = std::move(pathArguments.front());
            pathArguments.pop_front();
            try
            {
                if (v.empty() == false)
                    getHandlerArgumentValue(value, std::move(v));
            }
            catch (const std::exception &e)
            {
                handleException(e, req, std::move(callback));
                return;
            }
        }
        else
        {
            try
            {
                value = req->as<ValueType>();
            }
            catch (const std::exception &e)
            {
                handleException(e, req, std::move(callback));
                return;
            }
            catch (...)
            {
                LOG_ERROR << "Exception not derived from std::exception";
                return;
            }
        }

        run(pathArguments,
            req,
            std::move(callback),
            std::forward<Values>(values)...,
            std::move(value));
    }
    template <typename... Values,
              std::size_t Boundary = argument_count,
              bool isCoroutine = traits::isCoroutine>
    typename std::enable_if<(sizeof...(Values) == Boundary) && !isCoroutine,
                            void>::type
    run(std::deque<std::string> &,
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        Values &&...values)
    {
        try
        {
            // Explcit copy because `callFunction` moves it
            auto cb = callback;
            callFunction(req, cb, std::move(values)...);
        }
        catch (const std::exception &except)
        {
            handleException(except, req, std::move(callback));
        }
        catch (...)
        {
            LOG_ERROR << "Exception not derived from std::exception";
            return;
        }
    }
#ifdef __cpp_impl_coroutine
    template <typename... Values,
              std::size_t Boundary = argument_count,
              bool isCoroutine = traits::isCoroutine>
    typename std::enable_if<(sizeof...(Values) == Boundary) && isCoroutine,
                            void>::type
    run(std::deque<std::string> &,
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        Values &&...values)
    {
        [this](HttpRequestPtr req,
               std::function<void(const HttpResponsePtr &)> callback,
               Values &&...values) -> AsyncTask {
            try
            {
                if constexpr (std::is_same_v<AsyncTask,
                                             typename traits::return_type>)
                {
                    // Explcit copy because `callFunction` moves it
                    auto cb = callback;
                    callFunction(req, cb, std::move(values)...);
                }
                else if constexpr (std::is_same_v<Task<>,
                                                  typename traits::return_type>)
                {
                    // Explcit copy because `callFunction` moves it
                    auto cb = callback;
                    co_await callFunction(req, cb, std::move(values)...);
                }
                else if constexpr (std::is_same_v<Task<HttpResponsePtr>,
                                                  typename traits::return_type>)
                {
                    auto resp =
                        co_await callFunction(req, std::move(values)...);
                    callback(std::move(resp));
                }
            }
            catch (const std::exception &except)
            {
                handleException(except, req, std::move(callback));
            }
            catch (...)
            {
                LOG_ERROR << "Exception not derived from std::exception";
            }
        }(req, std::move(callback), std::move(values)...);
    }
#endif
    template <typename... Values,
              bool isClassFunction = traits::isClassFunction,
              bool isDrObjectClass = traits::isDrObjectClass,
              bool isNormal = std::is_same<typename traits::first_param_type,
                                           HttpRequestPtr>::value>
    typename std::enable_if<isClassFunction && !isDrObjectClass && isNormal,
                            typename traits::return_type>::type
    callFunction(const HttpRequestPtr &req, Values &&...values)
    {
        static auto &obj = getControllerObj<typename traits::class_type>();
        return (obj.*func_)(req, std::move(values)...);
    }
    template <typename... Values,
              bool isClassFunction = traits::isClassFunction,
              bool isDrObjectClass = traits::isDrObjectClass,
              bool isNormal = std::is_same<typename traits::first_param_type,
                                           HttpRequestPtr>::value>
    typename std::enable_if<isClassFunction && isDrObjectClass && isNormal,
                            typename traits::return_type>::type
    callFunction(const HttpRequestPtr &req, Values &&...values)
    {
        static auto objPtr =
            DrClassMap::getSingleInstance<typename traits::class_type>();
        return (*objPtr.*func_)(req, std::move(values)...);
    }
    template <typename... Values,
              bool isClassFunction = traits::isClassFunction,
              bool isNormal = std::is_same<typename traits::first_param_type,
                                           HttpRequestPtr>::value>
    typename std::enable_if<!isClassFunction && isNormal,
                            typename traits::return_type>::type
    callFunction(const HttpRequestPtr &req, Values &&...values)
    {
        return func_(req, std::move(values)...);
    }

    template <typename... Values,
              bool isClassFunction = traits::isClassFunction,
              bool isDrObjectClass = traits::isDrObjectClass,
              bool isNormal = std::is_same<typename traits::first_param_type,
                                           HttpRequestPtr>::value>
    typename std::enable_if<isClassFunction && !isDrObjectClass && !isNormal,
                            typename traits::return_type>::type
    callFunction(const HttpRequestPtr &req, Values &&...values)
    {
        static auto &obj = getControllerObj<typename traits::class_type>();
        return (obj.*func_)((*req), std::move(values)...);
    }
    template <typename... Values,
              bool isClassFunction = traits::isClassFunction,
              bool isDrObjectClass = traits::isDrObjectClass,
              bool isNormal = std::is_same<typename traits::first_param_type,
                                           HttpRequestPtr>::value>
    typename std::enable_if<isClassFunction && isDrObjectClass && !isNormal,
                            typename traits::return_type>::type
    callFunction(const HttpRequestPtr &req, Values &&...values)
    {
        static auto objPtr =
            DrClassMap::getSingleInstance<typename traits::class_type>();
        return (*objPtr.*func_)((*req), std::move(values)...);
    }
    template <typename... Values,
              bool isClassFunction = traits::isClassFunction,
              bool isNormal = std::is_same<typename traits::first_param_type,
                                           HttpRequestPtr>::value>
    typename std::enable_if<!isClassFunction && !isNormal,
                            typename traits::return_type>::type
    callFunction(const HttpRequestPtr &req, Values &&...values)
    {
        return func_((*req), std::move(values)...);
    }
};

}  // namespace internal
}  // namespace drogon
