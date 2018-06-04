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

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/utils/FunctionTraits.h>
#include <list>
#include <string>
#include <sstream>
#include <memory>
namespace drogon{

    class HttpApiBinderBase
    {
    public:
        virtual void handleHttpApiRequest(std::list<std::string> &pathParameter,
                                          const HttpRequest& req,std::function<void (HttpResponse &)>callback)
        =0;
        virtual ~HttpApiBinderBase(){}
    };
    typedef std::shared_ptr<HttpApiBinderBase> HttpApiBinderBasePtr;
    template <typename FUNCTION>
    class HttpApiBinder:public HttpApiBinderBase
    {
    public:
        typedef FUNCTION FunctionType;
        virtual void handleHttpApiRequest(std::list<std::string> &pathParameter,
                                          const HttpRequest& req,std::function<void (HttpResponse &)>callback) override
        {
            run(pathParameter,req,callback);
        }
        HttpApiBinder(FUNCTION &&func):
        _func(std::forward<FUNCTION>(func))
        {
            static_assert(traits::isHTTPApiFunction,"Your API handler function interface is wrong!");
        }
        void test(){
            std::cout<<"argument_count="<<argument_count<<" "<<traits::isHTTPApiFunction<<std::endl;
        }
    private:
        FUNCTION _func;

        typedef utility::FunctionTraits<FUNCTION> traits;
        template <
                std::size_t Index
        >
        using nth_argument_type = typename traits::template argument<Index>;


        static const size_t argument_count = traits::arity;

        template<
                typename... Values,
                std::size_t Boundary = argument_count
        >
        typename std::enable_if<(sizeof...(Values) < Boundary), void>::type run(
                std::list<std::string> &pathParameter,
                const HttpRequest& req,std::function<void (HttpResponse &)>callback,
                Values&&...      values
        ) {
            typedef typename std::remove_cv<typename std::remove_reference<nth_argument_type<sizeof...(Values)>>::type>::type ValueType;
            ValueType value;
            if(!pathParameter.empty())
            {
                std::string v=std::move(pathParameter.front());
                pathParameter.pop_front();

                std::stringstream ss(std::move(v));
                ss>>value;

            }

            run(pathParameter,req,callback, std::forward<Values>(values)..., std::move(value));
        }
        template<
                typename... Values,
                std::size_t Boundary = argument_count
        >
        typename std::enable_if<(sizeof...(Values) == Boundary), void>::type run(
                std::list<std::string> &pathParameter,
                const HttpRequest& req,std::function<void (HttpResponse &)>callback,
                Values&&...      values
        )
        {
            callFunction(req,callback,std::move(values)...);
        }
        template <typename... Values,
                bool isClassFunction = traits::isClassFunction>
        typename std::enable_if<isClassFunction,void>::type callFunction(
                const HttpRequest& req,std::function<void (HttpResponse &)>callback,
                Values&&... values)
        {
            //new object per time or create a object in constructor???
            std::unique_ptr<typename traits::class_type> ptr(new typename traits::class_type);
            (ptr.get()->*_func)(req,callback,std::move(values)...);
        };
        template <typename... Values,
                bool isClassFunction = traits::isClassFunction>
        typename std::enable_if<!isClassFunction,void>::type callFunction(
                const HttpRequest& req,std::function<void (HttpResponse &)>callback,
                Values&&... values)
        {
            _func(req,callback,std::move(values)...);
        };
    };
}