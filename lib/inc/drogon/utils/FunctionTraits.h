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

#include <tuple>
#include <type_traits>
#include <memory>
#include <functional>

namespace drogon
{
class HttpRequest;
class HttpResponse;
typedef std::shared_ptr<HttpRequest> HttpRequestPtr;
typedef std::shared_ptr<HttpResponse> HttpResponsePtr;

namespace utility
{

template <typename>
struct FunctionTraits;

//functor,lambda,std::function...
template <typename Function>
struct FunctionTraits : public FunctionTraits<
                            decltype(&std::remove_reference<Function>::type::operator())>
{
        static const bool isClassFunction = false;
        static const std::string name()
        {
                return std::string("Functor");
        }
};

//class instance method of const object
template <
    typename ClassType,
    typename ReturnType,
    typename... Arguments>
struct FunctionTraits<
    ReturnType (ClassType::*)(Arguments...) const> : FunctionTraits<ReturnType (*)(Arguments...)>
{
        static const bool isClassFunction = true;
        typedef ClassType class_type;
        static const std::string name() { return std::string("Class Function"); }
};

//class instance method of non-const object
template <
    typename ClassType,
    typename ReturnType,
    typename... Arguments>
struct FunctionTraits<
    ReturnType (ClassType::*)(Arguments...)> : FunctionTraits<ReturnType (*)(Arguments...)>
{
        static const bool isClassFunction = true;
        typedef ClassType class_type;
        static const std::string name() { return std::string("Class Function"); }
};

//normal function for HTTP handling
template <
    typename ReturnType,
    typename... Arguments>
struct FunctionTraits<
    ReturnType (*)(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback, Arguments...)> : FunctionTraits<ReturnType (*)(Arguments...)>
{
        static const bool isHTTPFunction = true;
};

//normal function
template <
    typename ReturnType,
    typename... Arguments>
struct FunctionTraits<
    ReturnType (*)(Arguments...)>
{
        typedef ReturnType result_type;

        template <std::size_t Index>
        using argument = typename std::tuple_element<
            Index,
            std::tuple<Arguments...>>::type;

        static const std::size_t arity = sizeof...(Arguments);

        static const bool isHTTPFunction = false;
        static const bool isClassFunction = false;
        static const std::string name() { return std::string("Normal or Static Function"); }
};

} // namespace utility
} // namespace drogon
