/**
 *
 *  ListenerManager.cc
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

#include "ListenerManager.h"
#include <drogon/config.h>
#include <trantor/utils/Logger.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace drogon
{
class DrogonFileLocker : public trantor::NonCopyable
{
  public:
    DrogonFileLocker()
    {
        _fd = open("/tmp/drogon.lock", O_TRUNC | O_CREAT, 0755);
        flock(_fd, LOCK_EX);
    }
    ~DrogonFileLocker()
    {
        close(_fd);
    }

  private:
    int _fd = 0;
};

}  // namespace drogon

using namespace trantor;
using namespace drogon;

void ListenerManager::addListener(const std::string &ip,
                                  uint16_t port,
                                  bool useSSL,
                                  const std::string &certFile,
                                  const std::string &keyFile)
{
#ifndef OpenSSL_FOUND
    if (useSSL)
    {
        LOG_ERROR << "Can't use SSL without OpenSSL found in your system";
    }
#endif
    _listeners.emplace_back(ip, port, useSSL, certFile, keyFile);
}

std::vector<trantor::EventLoop *> ListenerManager::createListeners(
    const HttpAsyncCallback &httpCallback,
    const WebSocketNewAsyncCallback &webSocketCallback,
    const ConnectionCallback &connectionCallback,
    size_t connectionTimeout,
    const std::string &globalCertFile,
    const std::string &globalKeyFile,
    size_t threadNum)
{
#ifdef __linux__
    std::vector<trantor::EventLoop *> ioLoops;
    for (size_t i = 0; i < threadNum; i++)
    {
        LOG_TRACE << "thread num=" << threadNum;
        auto loopThreadPtr = std::make_shared<EventLoopThread>("DrogonIoLoop");
        _listeningloopThreads.push_back(loopThreadPtr);
        ioLoops.push_back(loopThreadPtr->getLoop());
        for (auto const &listener : _listeners)
        {
            auto const &ip = listener._ip;
            bool isIpv6 = ip.find(":") == std::string::npos ? false : true;
            std::shared_ptr<HttpServer> serverPtr;
            if (i == 0)
            {
                DrogonFileLocker lock;
                // Check whether the port is in use.
                TcpServer server(app().getLoop(),
                                 InetAddress(ip, listener._port, isIpv6),
                                 "drogonPortTest",
                                 true,
                                 false);
                serverPtr = std::make_shared<HttpServer>(
                    loopThreadPtr->getLoop(),
                    InetAddress(ip, listener._port, isIpv6),
                    "drogon");
            }
            else
            {
                serverPtr = std::make_shared<HttpServer>(
                    loopThreadPtr->getLoop(),
                    InetAddress(ip, listener._port, isIpv6),
                    "drogon");
            }

            if (listener._useSSL)
            {
#ifdef OpenSSL_FOUND
                auto cert = listener._certFile;
                auto key = listener._keyFile;
                if (cert == "")
                    cert = globalCertFile;
                if (key == "")
                    key = globalKeyFile;
                if (cert == "" || key == "")
                {
                    std::cerr
                        << "You can't use https without cert file or key file"
                        << std::endl;
                    exit(1);
                }
                serverPtr->enableSSL(cert, key);
#endif
            }
            serverPtr->setHttpAsyncCallback(httpCallback);
            serverPtr->setNewWebsocketCallback(webSocketCallback);
            serverPtr->setConnectionCallback(connectionCallback);
            serverPtr->kickoffIdleConnections(connectionTimeout);
            serverPtr->start();
            _servers.push_back(serverPtr);
        }
    }
#else
    auto loopThreadPtr =
        std::make_shared<EventLoopThread>("DrogonListeningLoop");
    _listeningloopThreads.push_back(loopThreadPtr);
    _ioLoopThreadPoolPtr = std::make_shared<EventLoopThreadPool>(threadNum);
    for (auto const &listener : _listeners)
    {
        LOG_TRACE << "thread num=" << threadNum;
        auto ip = listener._ip;
        bool isIpv6 = ip.find(":") == std::string::npos ? false : true;
        auto serverPtr = std::make_shared<HttpServer>(
            loopThreadPtr->getLoop(),
            InetAddress(ip, listener._port, isIpv6),
            "drogon");
        if (listener._useSSL)
        {
#ifdef OpenSSL_FOUND
            auto cert = listener._certFile;
            auto key = listener._keyFile;
            if (cert == "")
                cert = globalCertFile;
            if (key == "")
                key = globalKeyFile;
            if (cert == "" || key == "")
            {
                std::cerr << "You can't use https without cert file or key file"
                          << std::endl;
                exit(1);
            }
            serverPtr->enableSSL(cert, key);
#endif
        }
        serverPtr->setIoLoopThreadPool(_ioLoopThreadPoolPtr);
        serverPtr->setHttpAsyncCallback(httpCallback);
        serverPtr->setNewWebsocketCallback(webSocketCallback);
        serverPtr->setConnectionCallback(connectionCallback);
        serverPtr->kickoffIdleConnections(connectionTimeout);
        serverPtr->start();
        _servers.push_back(serverPtr);
    }
    auto ioLoops = _ioLoopThreadPoolPtr->getLoops();
#endif
    return ioLoops;
}

void ListenerManager::startListening()
{
    for (auto &loopThread : _listeningloopThreads)
    {
        loopThread->run();
    }
}