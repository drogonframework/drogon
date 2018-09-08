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
#include <memory>
namespace drogon{
    enum class ReqResult{
        Ok,
        BadResponse,
        NetworkFailure,
        BadServerAddress,
        Timeout
    };
    typedef std::function<void(ReqResult,const HttpResponse &response)> HttpReqCallback;
    class HttpClient;
    typedef std::shared_ptr<HttpClient> HttpClientPtr;
    class HttpClient:public trantor::NonCopyable
    {
    public:
        virtual void sendRequest(const HttpRequestPtr &req,const HttpReqCallback &callback)=0;
        virtual ~HttpClient(){}
        static HttpClientPtr newHttpClient(const std::string &ip,uint16_t port,bool useSSL=false) ;
        static HttpClientPtr newHttpClient(const trantor::InetAddress &addr,bool useSSL=false) ;
        static HttpClientPtr newHttpClient(const std::string &hostString);
    protected:
        HttpClient()= default;
    };
}