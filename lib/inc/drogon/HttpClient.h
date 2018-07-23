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
#include <trantor/utils/NonCopyable.h>
#include <functional>
namespace drogon{
    enum class ReqResult{
        Ok,
        NetworkFailure,
        Timeout
    };
    typedef std::function<void(ReqResult,const HttpResponsePtr &response)> HttpReqCallback;
    class HttpClient:public trantor::NonCopyable
    {
    public:
        virtual void sendRequest(const HttpRequest &req,const HttpReqCallback &callback)=0;
        virtual ~HttpClient(){}

    protected:
        HttpClient()= default;
    };
}