/**
 *
 *  FunctionTraits.h
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

#include <drogon/DrObject.h>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>

#ifdef __cpp_impl_coroutine
#include <drogon/utils/coroutine.h>
#endif

namespace drogon
{
class HttpRequest;
class HttpResponse;
using HttpRequestPtr = std::shared_ptr<HttpRequest>;
using HttpResponsePtr = std::shared_ptr<HttpResponse>;

namespace internal
{
#ifdef __cpp_impl_coroutine
template <typename T>
using resumable_type = is_resumable<T>;
#else
template <typename T>
struct resumable_type : std::false_type
{
};
#endif

template <typename>
struct FunctionTraits;

// functor,lambda,std::function...
template <typename Function>
struct FunctionTraits : public FunctionTraits<decltype(
                            &std::remove_reference<Function>::type::operator())>
{
    static const bool isClassFunction = false;
    static const bool isDrObjectClass = false;
    using class_type = void;
    static const std::string name()
    {
        return std::string("Functor");
    }
};

// class instance method of const object
template <typename ClassType, typename ReturnType, typename... Arguments>
struct FunctionTraits<ReturnType (ClassType::*)(Arguments...) const>
    : FunctionTraits<ReturnType (*)(Arguments...)>
{
    static const bool isClassFunction = true;
    static const bool isDrObjectClass =
        std::is_base_of<DrObject<ClassType>, ClassType>::value;
    using class_type = ClassType;
    static const std::string name()
    {
        return std::string("Class Function");
    }
};

// class instance method of non-const object
template <typename ClassType, typename ReturnType, typename... Arguments>
struct FunctionTraits<ReturnType (ClassType::*)(Arguments...)>
    : FunctionTraits<ReturnType (*)(Arguments...)>
{
    static const bool isClassFunction = true;
    static const bool isDrObjectClass =
        std::is_base_of<DrObject<ClassType>, ClassType>::value;
    using class_type = ClassType;
    static const std::string name()
    {
        return std::string("Class Function");
    }
};

// normal function for HTTP handling
template <typename ReturnType, typename... Arguments>
struct FunctionTraits<
    ReturnType (*)(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   Arguments...)> : FunctionTraits<ReturnType (*)(Arguments...)>
{
    static const bool isHTTPFunction = !resumable_type<ReturnType>::value;
    static const bool isCoroutine = false;
    using class_type = void;
    using first_param_type = HttpRequestPtr;
    using return_type = ReturnType;
};

template <typename ReturnType, typename... Arguments>
struct FunctionTraits<
    ReturnType (*)(HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   Arguments...)> : FunctionTraits<ReturnType (*)(Arguments...)>
{
    static const bool isHTTPFunction = false;
    using class_type = void;
};

#ifdef __cpp_impl_coroutine
template <typename... Arguments>
struct FunctionTraits<
    AsyncTask (*)(HttpRequestPtr req,
                  std::function<void(const HttpResponsePtr &)> callback,
                  Arguments...)> : FunctionTraits<AsyncTask (*)(Arguments...)>
{
    static const bool isHTTPFunction = true;
    static const bool isCoroutine = true;
    using class_type = void;
    using first_param_type = HttpRequestPtr;
    using return_type = AsyncTask;
};
template <typename... Arguments>
struct FunctionTraits<
    Task<> (*)(HttpRequestPtr req,
               std::function<void(const HttpResponsePtr &)> callback,
               Arguments...)> : FunctionTraits<AsyncTask (*)(Arguments...)>
{
    static const bool isHTTPFunction = true;
    static const bool isCoroutine = true;
    using class_type = void;
    using first_param_type = HttpRequestPtr;
    using return_type = Task<>;
};
template <typename... Arguments>
struct FunctionTraits<Task<HttpResponsePtr> (*)(HttpRequestPtr req,
                                                Arguments...)>
    : FunctionTraits<AsyncTask (*)(Arguments...)>
{
    static const bool isHTTPFunction = true;
    static const bool isCoroutine = true;
    using class_type = void;
    using first_param_type = HttpRequestPtr;
    using return_type = Task<HttpResponsePtr>;
};
#endif

template <typename ReturnType, typename... Arguments>
struct FunctionTraits<
    ReturnType (*)(HttpRequestPtr &&req,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   Arguments...)> : FunctionTraits<ReturnType (*)(Arguments...)>
{
    static const bool isHTTPFunction = false;
    using class_type = void;
};

// normal function for HTTP handling
template <typename T, typename ReturnType, typename... Arguments>
struct FunctionTraits<
    ReturnType (*)(T &&customReq,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   Arguments...)> : FunctionTraits<ReturnType (*)(Arguments...)>
{
    static const bool isHTTPFunction = !resumable_type<ReturnType>::value;
    static const bool isCoroutine = false;
    using class_type = void;
    using first_param_type = T;
    using return_type = ReturnType;
};

// normal function
template <typename ReturnType, typename... Arguments>
struct FunctionTraits<ReturnType (*)(Arguments...)>
{
    using result_type = ReturnType;

    template <std::size_t Index>
    using argument =
        typename std::tuple_element<Index, std::tuple<Arguments...>>::type;

    static const std::size_t arity = sizeof...(Arguments);
    using class_type = void;
    using return_type = ReturnType;
    static const bool isHTTPFunction = false;
    static const bool isClassFunction = false;
    static const bool isDrObjectClass = false;
    static const bool isCoroutine = false;
    static const std::string name()
    {
        return std::string("Normal or Static Function");
    }
};

}  // namespace internal
}  // namespace drogon
