/**
 *
 *  HttpBinder.h
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
#include <drogon/DrObject.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/utils/FunctionTraits.h>
#include <list>
#include <memory>
#include <sstream>
#include <string>

/// The classes in the file are internal tool classes. Do not include this
/// file directly and use any of these classes directly.

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
        std::list<std::string> &pathParameter,
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

typedef std::shared_ptr<HttpBinderBase> HttpBinderBasePtr;
template <typename FUNCTION>
class HttpBinder : public HttpBinderBase
{
  public:
    typedef FUNCTION FunctionType;
    virtual void handleHttpRequest(
        std::list<std::string> &pathParameter,
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) override
    {
        run(pathParameter, req, std::move(callback));
    }
    virtual size_t paramCount() override
    {
        return traits::arity;
    }
    HttpBinder(FUNCTION &&func) : _func(std::forward<FUNCTION>(func))
    {
        static_assert(traits::isHTTPFunction,
                      "Your API handler function interface is wrong!");
        _handlerName = DrClassMap::demangle(typeid(FUNCTION).name());
    }
    void test()
    {
        std::cout << "argument_count=" << argument_count << " "
                  << traits::isHTTPFunction << std::endl;
    }
    virtual const std::string &handlerName() const override
    {
        return _handlerName;
    }

  private:
    FUNCTION _func;

    typedef FunctionTraits<FUNCTION> traits;
    template <std::size_t Index>
    using nth_argument_type = typename traits::template argument<Index>;

    static const size_t argument_count = traits::arity;
    std::string _handlerName;
    template <typename... Values, std::size_t Boundary = argument_count>
    typename std::enable_if<(sizeof...(Values) < Boundary), void>::type run(
        std::list<std::string> &pathParameter,
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        Values &&... values)
    {
        // call this function recursively until parameter's count equals to the
        // count of target function parameters
        static_assert(BinderArgTypeTraits<
                          nth_argument_type<sizeof...(Values)>>::isValid,
                      "your handler argument type must be value type or const "
                      "left "
                      "reference "
                      "type or right reference type");
        typedef typename std::remove_cv<typename std::remove_reference<
            nth_argument_type<sizeof...(Values)>>::type>::type ValueType;
        ValueType value = ValueType();
        if (!pathParameter.empty())
        {
            std::string v = std::move(pathParameter.front());
            pathParameter.pop_front();
            if (!v.empty())
            {
                std::stringstream ss(std::move(v));
                ss >> value;
            }
        }

        run(pathParameter,
            req,
            std::move(callback),
            std::forward<Values>(values)...,
            std::move(value));
    }
    template <typename... Values, std::size_t Boundary = argument_count>
    typename std::enable_if<(sizeof...(Values) == Boundary), void>::type run(
        std::list<std::string> &pathParameter,
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        Values &&... values)
    {
        callFunction(req, std::move(callback), std::move(values)...);
    }
    template <typename... Values,
              bool isClassFunction = traits::isClassFunction,
              bool isDrObjectClass = traits::isDrObjectClass>
    typename std::enable_if<isClassFunction && !isDrObjectClass, void>::type
    callFunction(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback,
                 Values &&... values)
    {
        static auto &obj = getControllerObj<typename traits::class_type>();
        (obj.*_func)(req, std::move(callback), std::move(values)...);
    };
    template <typename... Values,
              bool isClassFunction = traits::isClassFunction,
              bool isDrObjectClass = traits::isDrObjectClass>
    typename std::enable_if<isClassFunction && isDrObjectClass, void>::type
    callFunction(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback,
                 Values &&... values)
    {
        static auto objPtr =
            DrClassMap::getSingleInstance<typename traits::class_type>();
        (*objPtr.*_func)(req, std::move(callback), std::move(values)...);
    };
    template <typename... Values,
              bool isClassFunction = traits::isClassFunction>
    typename std::enable_if<!isClassFunction, void>::type callFunction(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        Values &&... values)
    {
        _func(req, std::move(callback), std::move(values)...);
    };
};

}  // namespace internal
}  // namespace drogon
