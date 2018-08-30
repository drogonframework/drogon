/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#pragma once

#include <tuple>
#include<type_traits>
namespace drogon{
    class HttpRequest;
    class HttpResponse;
    typedef std::shared_ptr<HttpRequest> HttpRequestPtr;
    namespace utility {

        template<typename> struct FunctionTraits;

        template <typename Function>
        struct FunctionTraits : public FunctionTraits<
                decltype(&std::remove_reference<Function>::type::operator())
        > {
            static const bool isClassFunction=false;
            static const std::string name()
            {return std::string("Functor");}
        };


        template <
                typename    ClassType,
                typename    ReturnType,
                typename... Arguments
        >
        struct FunctionTraits<
                ReturnType(ClassType::*)(Arguments...) const
        > : FunctionTraits<ReturnType(*)(Arguments...)> {
            static const bool isClassFunction=true;
            typedef ClassType class_type;
            static const std::string name(){return std::string("Class Function");}
            };

        /* support the non-const operator ()
         * this will work with user defined functors */
        template <
                typename    ClassType,
                typename    ReturnType,
                typename... Arguments
        >
        struct FunctionTraits<
                ReturnType(ClassType::*)(Arguments...)
        > : FunctionTraits<ReturnType(*)(Arguments...)> {
            static const bool isClassFunction=true;
            typedef ClassType class_type;
            static const std::string name(){return std::string("Class Function");}
        };

        //class function
        template <
                typename    ClassType,
                typename    ReturnType,
                typename... Arguments
        >
        struct FunctionTraits<
                ReturnType(ClassType::*)(const HttpRequestPtr& req,const std::function<void (const HttpResponsePtr &)>&callback,Arguments...) const
        > : FunctionTraits<ReturnType(ClassType::*)(Arguments...)> {
            static const bool isHTTPApiFunction=true;
            static const std::string name(){return std::string("Class Const Api Function");}
        };

        template <
                typename    ClassType,
                typename    ReturnType,
                typename... Arguments
        >
        struct FunctionTraits<
                ReturnType(ClassType::*)(const HttpRequestPtr& req,const std::function<void (const HttpResponsePtr &)>&callback,Arguments...)
        > : FunctionTraits<ReturnType(ClassType::*)(Arguments...)> {
            static const bool isHTTPApiFunction=true;
            static const std::string name(){return std::string("Class Api Function");}
            };
        //normal function
        template <
                typename    ReturnType,
                typename... Arguments
        >
        struct FunctionTraits<
                ReturnType(*)(const HttpRequestPtr& req,const std::function<void (const HttpResponsePtr &)>&callback,Arguments...)
        > : FunctionTraits<ReturnType(*)(Arguments...)> {
            static const bool isHTTPApiFunction=true;

        };
        //std::function
//        template <
//                typename    ReturnType,
//                typename... Arguments
//        >
//        struct FunctionTraits<std::function<
//                ReturnType(const HttpRequest& req,const std::function<void (const HttpResponsePtr &)>& callback,Arguments...)>>
//                :FunctionTraits<ReturnType(*)(Arguments...)> {
//            static const bool isHTTPApiFunction=true;
//            static const std::string name(){return std::string("std::function");}
//        };

        template <
                typename    ReturnType,
                typename... Arguments
        >
        struct FunctionTraits<
                ReturnType(*)(Arguments...)
        > {
            typedef ReturnType result_type;

            template <std::size_t Index>
            using argument = typename std::tuple_element<
                    Index,
                    std::tuple<Arguments...>
            >::type;

            static const std::size_t arity = sizeof...(Arguments);

            static const bool isHTTPApiFunction=false;
            static const bool isClassFunction=false;
            static const std::string name(){return std::string("Normal or Static Function");}
        };

    }
}
