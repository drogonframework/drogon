// Copyright 2018,antao.  All rights reserved.


#pragma once

#include <trantor/net/TcpServer.h>
#include <trantor/utils/NonCopyable.h>
#include <functional>
#include <string>

class HttpRequest;
class HttpResponse;
using namespace trantor;
namespace drogon
{
    class HttpServer : trantor::NonCopyable
    {
    public:

        typedef std::function< void (const HttpRequest&,std::function<void (HttpResponse &)>)> HttpAsyncCallback;
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
