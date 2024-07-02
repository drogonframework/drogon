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

template <typename T>
T getHandlerArgumentValue(std::string &&p)
{
    if constexpr (internal::CanConstructFromString<T>::value)
    {
        return T(std::move(p));
    }
    else if constexpr (internal::CanConvertFromStringStream<T>::value)
    {
        T value{T()};
        if (!p.empty())
        {
            std::stringstream ss(std::move(p));
            ss >> value;
        }
        return value;
    }
    else if constexpr (internal::CanConvertFromString<T>::value)
    {
        T value;
        value = p;
        return value;
    }
    else
    {
        LOG_ERROR << "Can't convert string to type " << typeid(T).name();
        return T();
    }
}

template <>
inline std::string getHandlerArgumentValue<std::string>(std::string &&p)
{
    return std::move(p);
}

template <>
inline int getHandlerArgumentValue<int>(std::string &&p)
{
    return std::stoi(p);
}

template <>
inline long getHandlerArgumentValue<long>(std::string &&p)
{
    return std::stol(p);
}

template <>
inline long long getHandlerArgumentValue<long long>(std::string &&p)
{
    return std::stoll(p);
}

template <>
inline unsigned long getHandlerArgumentValue<unsigned long>(std::string &&p)
{
    return std::stoul(p);
}

template <>
inline unsigned long long getHandlerArgumentValue<unsigned long long>(
    std::string &&p)
{
    return std::stoull(p);
}

template <>
inline float getHandlerArgumentValue<float>(std::string &&p)
{
    return std::stof(p);
}

template <>
inline double getHandlerArgumentValue<double>(std::string &&p)
{
    return std::stod(p);
}

template <>
inline long double getHandlerArgumentValue<long double>(std::string &&p)
{
    return std::stold(p);
}

class HttpBinderBase
{
  public:
    virtual void handleHttpRequest(
        std::deque<std::string> &pathArguments,
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) = 0;
    virtual size_t paramCount() = 0;
    virtual const std::string &handlerName() const = 0;
    virtual bool isStreamHandler() = 0;

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
        if (!pathArguments.empty())
        {
            std::vector<std::string> args;
            args.reserve(pathArguments.size());
            for (auto &arg : pathArguments)
            {
                args.emplace_back(arg);
            }
            req->setRoutingParameters(std::move(args));
        }
        run(pathArguments, req, std::move(callback));
    }

    size_t paramCount() override
    {
        return traits::arity;
    }

    bool isStreamHandler() override
    {
        return traits::isStreamHandler;
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
    void createHandlerInstance()
    {
        if constexpr (isClassFunction)
        {
            if constexpr (isDrObjectClass)
            {
                auto objPtr = DrClassMap::getSingleInstance<
                    typename traits::class_type>();
                LOG_TRACE << "create handler class object: " << objPtr.get();
            }
            else
            {
                auto &obj = getControllerObj<typename traits::class_type>();
                LOG_TRACE << "create handler class object: " << &obj;
            }
        }
    }

  private:
    FUNCTION func_;
    template <std::size_t Index>
    using nth_argument_type = typename traits::template argument<Index>;

    static const size_t argument_count = traits::arity;
    std::string handlerName_;

    template <typename... Values,
              std::size_t Boundary = argument_count,
              bool isStreamHandler = traits::isStreamHandler,
              bool isCoroutine = traits::isCoroutine>
    void run(std::deque<std::string> &pathArguments,
             const HttpRequestPtr &req,
             std::function<void(const HttpResponsePtr &)> &&callback,
             Values &&...values)
    {
        if constexpr (sizeof...(Values) < Boundary)
        {  // Call this function recursively until parameter's count equals to
           // the count of target function parameters
            static_assert(
                BinderArgTypeTraits<
                    nth_argument_type<sizeof...(Values)>>::isValid,
                "your handler argument type must be value type or const left "
                "reference type or right reference type");
            using ValueType = std::remove_cv_t<
                std::remove_reference_t<nth_argument_type<sizeof...(Values)>>>;
            if (!pathArguments.empty())
            {
                std::string v{std::move(pathArguments.front())};
                pathArguments.pop_front();
                try
                {
                    if (!v.empty())
                    {
                        auto value =
                            getHandlerArgumentValue<ValueType>(std::move(v));
                        run(pathArguments,
                            req,
                            std::move(callback),
                            std::forward<Values>(values)...,
                            std::move(value));
                        return;
                    }
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
                    auto value = req->as<ValueType>();
                    run(pathArguments,
                        req,
                        std::move(callback),
                        std::forward<Values>(values)...,
                        std::move(value));
                    return;
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
                ValueType());
        }
        else if constexpr (sizeof...(Values) == Boundary)
        {
            if constexpr (!isCoroutine)
            {
                try
                {
                    // Explicit copy because `callFunction` moves it
                    auto cb = callback;
                    if constexpr (isStreamHandler)
                    {
                        callFunction(req,
                                     createRequestStream(req),
                                     cb,
                                     std::move(values)...);
                    }
                    else
                    {
                        callFunction(req, cb, std::move(values)...);
                    }
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
            else
            {
                static_assert(!isStreamHandler);
                [this](HttpRequestPtr req,
                       std::function<void(const HttpResponsePtr &)> callback,
                       Values &&...values) -> AsyncTask {
                    try
                    {
                        if constexpr (std::is_same_v<
                                          AsyncTask,
                                          typename traits::return_type>)
                        {
                            // Explicit copy because `callFunction` moves it
                            auto cb = callback;
                            callFunction(req, cb, std::move(values)...);
                        }
                        else if constexpr (std::is_same_v<
                                               Task<>,
                                               typename traits::return_type>)
                        {
                            // Explicit copy because `callFunction` moves it
                            auto cb = callback;
                            co_await callFunction(req,
                                                  cb,
                                                  std::move(values)...);
                        }
                        else if constexpr (std::is_same_v<
                                               Task<HttpResponsePtr>,
                                               typename traits::return_type>)
                        {
                            auto resp =
                                co_await callFunction(req,
                                                      std::move(values)...);
                            callback(std::move(resp));
                        }
                    }
                    catch (const std::exception &except)
                    {
                        handleException(except, req, std::move(callback));
                    }
                    catch (...)
                    {
                        LOG_ERROR
                            << "Exception not derived from std::exception";
                    }
                    co_return;
                }(req, std::move(callback), std::move(values)...);
            }
#endif
        }
    }

    template <typename... Values,
              bool isClassFunction = traits::isClassFunction,
              bool isDrObjectClass = traits::isDrObjectClass,
              bool isNormal = std::is_same_v<typename traits::first_param_type,
                                             HttpRequestPtr>>
    typename traits::return_type callFunction(const HttpRequestPtr &req,
                                              Values &&...values)
    {
        if constexpr (isNormal)
        {
            if constexpr (isClassFunction)
            {
                if constexpr (!isDrObjectClass)
                {
                    static auto &obj =
                        getControllerObj<typename traits::class_type>();
                    return (obj.*func_)(req, std::move(values)...);
                }
                else
                {
                    static auto objPtr = DrClassMap::getSingleInstance<
                        typename traits::class_type>();
                    return (*objPtr.*func_)(req, std::move(values)...);
                }
            }
            else
            {
                return func_(req, std::move(values)...);
            }
        }
        else
        {
            if constexpr (isClassFunction)
            {
                if constexpr (!isDrObjectClass)
                {
                    static auto &obj =
                        getControllerObj<typename traits::class_type>();
                    return (obj.*func_)((*req), std::move(values)...);
                }
                else
                {
                    static auto objPtr = DrClassMap::getSingleInstance<
                        typename traits::class_type>();
                    return (*objPtr.*func_)((*req), std::move(values)...);
                }
            }
            else
            {
                return func_((*req), std::move(values)...);
            }
        }
    }
};

}  // namespace internal
}  // namespace drogon
