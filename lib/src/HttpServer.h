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

#include <drogon/config.h>
#include <trantor/net/TcpServer.h>
#include <trantor/utils/NonCopyable.h>
#include <functional>
#include <string>



using namespace trantor;
namespace drogon
{
    class HttpRequest;
    class HttpResponse;
    class HttpServer : trantor::NonCopyable
    {
    public:

        typedef std::function< void (const HttpRequest&,const std::function<void (HttpResponse &)>&)> HttpAsyncCallback;
        HttpServer(EventLoop* loop,
                   const InetAddress& listenAddr,
                   const std::string& name);

        ~HttpServer();

        EventLoop* getLoop() const { return server_.getLoop(); }

        void setHttpAsyncCallback(const HttpAsyncCallback& cb)
        {
            httpAsyncCallback_= cb;
        }

        void setIoLoopNum(int numThreads)
        {
            server_.setIoLoopNum(numThreads);
        }

        void start();

    private:
        void onConnection(const TcpConnectionPtr& conn);
        void onMessage(const TcpConnectionPtr&,
                       MsgBuffer*);
        void onRequest(const TcpConnectionPtr&, const HttpRequest&);
        trantor::TcpServer server_;
        HttpAsyncCallback httpAsyncCallback_;

    };



}
