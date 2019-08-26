/**
 *
 *  ListenerManager.h
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

#include "impl_forwards.h"
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoopThreadPool.h>
#include <trantor/net/callbacks.h>
#include <string>
#include <vector>
#include <memory>

namespace drogon
{
class ListenerManager : public trantor::NonCopyable
{
  public:
    void addListener(const std::string &ip,
                     uint16_t port,
                     bool useSSL = false,
                     const std::string &certFile = "",
                     const std::string &keyFile = "");
    std::vector<trantor::EventLoop *> createListeners(
        const HttpAsyncCallback &httpCallback,
        const WebSocketNewAsyncCallback &webSocketCallback,
        const trantor::ConnectionCallback &connectionCallback,
        size_t connectionTimeout,
        const std::string &globalCertFile,
        const std::string &globalKeyFile,
        size_t threadNum,
        const std::vector<
            std::function<HttpResponsePtr(const HttpRequestPtr &)>>
            &syncAdvices);
    void startListening();

  private:
    struct ListenerInfo
    {
        ListenerInfo(const std::string &ip,
                     uint16_t port,
                     bool useSSL,
                     const std::string &certFile,
                     const std::string &keyFile)
            : _ip(ip),
              _port(port),
              _useSSL(useSSL),
              _certFile(certFile),
              _keyFile(keyFile)
        {
        }
        std::string _ip;
        uint16_t _port;
        bool _useSSL;
        std::string _certFile;
        std::string _keyFile;
    };
    std::vector<ListenerInfo> _listeners;
    std::vector<std::shared_ptr<HttpServer>> _servers;
    std::vector<std::shared_ptr<trantor::EventLoopThread>>
        _listeningloopThreads;
    std::string _sslCertPath;
    std::string _sslKeyPath;
    std::shared_ptr<trantor::EventLoopThreadPool> _ioLoopThreadPoolPtr;
};

}  // namespace drogon