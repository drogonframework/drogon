/**
 *
 *  @file ListenerManager.cc
 *  @author An Tao
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
#include "HttpServer.h"
#include "HttpAppFrameworkImpl.h"
#include <drogon/config.h>
#include <trantor/utils/Logger.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/wait.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace drogon
{
#ifndef _WIN32
class DrogonFileLocker : public trantor::NonCopyable
{
  public:
    DrogonFileLocker()
    {
        fd_ = open("/tmp/drogon.lock", O_TRUNC | O_CREAT, 0666);
        flock(fd_, LOCK_EX);
    }
    ~DrogonFileLocker()
    {
        close(fd_);
    }

  private:
    int fd_{0};
};

#endif
}  // namespace drogon

using namespace trantor;
using namespace drogon;

void ListenerManager::addListener(
    const std::string &ip,
    uint16_t port,
    bool useSSL,
    const std::string &certFile,
    const std::string &keyFile,
    bool useOldTLS,
    const std::vector<std::pair<std::string, std::string>> &sslConfCmds)
{
#ifndef OpenSSL_FOUND
    if (useSSL)
    {
        LOG_ERROR << "Can't use SSL without OpenSSL found in your system";
    }
#endif
    listeners_.emplace_back(
        ip, port, useSSL, certFile, keyFile, useOldTLS, sslConfCmds);
}

std::vector<trantor::EventLoop *> ListenerManager::createListeners(
    const HttpAsyncCallback &httpCallback,
    const WebSocketNewAsyncCallback &webSocketCallback,
    const ConnectionCallback &connectionCallback,
    size_t connectionTimeout,
#ifdef OpenSSL_FOUND
    const std::string &globalCertFile,
    const std::string &globalKeyFile,
    const std::vector<std::pair<std::string, std::string>> &sslConfCmds,
#else
    const std::string & /*globalCertFile*/,
    const std::string & /*globalKeyFile*/,
    const std::vector<std::pair<std::string, std::string>> & /*sslConfCmds*/,
#endif
    size_t threadNum,
    const std::vector<std::function<HttpResponsePtr(const HttpRequestPtr &)>>
        &syncAdvices,
    const std::vector<
        std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>>
        &preSendingAdvices)
{
#ifdef __linux__
    for (size_t i = 0; i < threadNum; ++i)
    {
        LOG_TRACE << "thread num=" << threadNum;
        auto loopThreadPtr = std::make_shared<EventLoopThread>("DrogonIoLoop");
        listeningloopThreads_.push_back(loopThreadPtr);
        ioLoops_.push_back(loopThreadPtr->getLoop());
        for (auto const &listener : listeners_)
        {
            auto const &ip = listener.ip_;
            bool isIpv6 = ip.find(':') == std::string::npos ? false : true;
            std::shared_ptr<HttpServer> serverPtr;
            InetAddress listenAddress(ip, listener.port_, isIpv6);
            if (listenAddress.isUnspecified())
            {
                LOG_FATAL << "Failed to parse IP address '" << ip
                          << "'. (Note: FQDN/domain names/hostnames are not "
                             "supported. Including 'localhost')";
                abort();
            }
            if (i == 0 && !app().reusePort())
            {
                DrogonFileLocker lock;
                // Check whether the port is in use.
                TcpServer server(HttpAppFrameworkImpl::instance().getLoop(),
                                 std::move(listenAddress),
                                 "drogonPortTest",
                                 true,
                                 false);
                serverPtr =
                    std::make_shared<HttpServer>(loopThreadPtr->getLoop(),
                                                 std::move(listenAddress),
                                                 "drogon",
                                                 syncAdvices,
                                                 preSendingAdvices);
            }
            else
            {
                serverPtr =
                    std::make_shared<HttpServer>(loopThreadPtr->getLoop(),
                                                 std::move(listenAddress),
                                                 "drogon",
                                                 syncAdvices,
                                                 preSendingAdvices);
            }

            if (listener.useSSL_)
            {
#ifdef OpenSSL_FOUND
                auto cert = listener.certFile_;
                auto key = listener.keyFile_;
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
                auto cmds = sslConfCmds;
                std::copy(listener.sslConfCmds_.begin(),
                          listener.sslConfCmds_.end(),
                          std::back_inserter(cmds));
                serverPtr->enableSSL(cert, key, listener.useOldTLS_, cmds);
#endif
            }
            serverPtr->setHttpAsyncCallback(httpCallback);
            serverPtr->setNewWebsocketCallback(webSocketCallback);
            serverPtr->setConnectionCallback(connectionCallback);
            serverPtr->kickoffIdleConnections(connectionTimeout);
            serverPtr->start();
            servers_.push_back(serverPtr);
        }
    }
#else

    ioLoopThreadPoolPtr_ = std::make_shared<EventLoopThreadPool>(threadNum);
    if (!listeners_.empty())
    {
        auto loopThreadPtr =
            std::make_shared<EventLoopThread>("DrogonListeningLoop");
        listeningloopThreads_.push_back(loopThreadPtr);
        for (auto const &listener : listeners_)
        {
            LOG_TRACE << "thread num=" << threadNum;
            auto ip = listener.ip_;
            bool isIpv6 = ip.find(':') == std::string::npos ? false : true;
            auto serverPtr = std::make_shared<HttpServer>(
                loopThreadPtr->getLoop(),
                InetAddress(ip, listener.port_, isIpv6),
                "drogon",
                syncAdvices,
                preSendingAdvices);
            if (listener.useSSL_)
            {
#ifdef OpenSSL_FOUND
                auto cert = listener.certFile_;
                auto key = listener.keyFile_;
                if (cert.empty())
                    cert = globalCertFile;
                if (key.empty())
                    key = globalKeyFile;
                if (cert.empty() || key.empty())
                {
                    std::cerr
                        << "You can't use https without cert file or key file"
                        << std::endl;
                    exit(1);
                }
                auto cmds = sslConfCmds;
                std::copy(listener.sslConfCmds_.begin(),
                          listener.sslConfCmds_.end(),
                          std::back_inserter(cmds));
                serverPtr->enableSSL(cert, key, listener.useOldTLS_, cmds);
#endif
            }
            serverPtr->setIoLoopThreadPool(ioLoopThreadPoolPtr_);
            serverPtr->setHttpAsyncCallback(httpCallback);
            serverPtr->setNewWebsocketCallback(webSocketCallback);
            serverPtr->setConnectionCallback(connectionCallback);
            serverPtr->kickoffIdleConnections(connectionTimeout);
            serverPtr->start();
            servers_.push_back(serverPtr);
        }
    }
    else
    {
        ioLoopThreadPoolPtr_->start();
    }
    ioLoops_ = ioLoopThreadPoolPtr_->getLoops();
#endif
    return ioLoops_;
}

void ListenerManager::startListening()
{
    for (auto &loopThread : listeningloopThreads_)
    {
        loopThread->run();
    }
}

trantor::EventLoop *ListenerManager::getIOLoop(size_t id) const
{
#ifdef __linux__
    auto const n = listeningloopThreads_.size();
#else
    auto const n = ioLoopThreadPoolPtr_->size();
#endif
    if (0 == n)
    {
        LOG_WARN << "Please call getIOLoop() after drogon::app().run()";
        return nullptr;
    }
    if (id >= n)
    {
        LOG_TRACE << "Loop id (" << id << ") out of range [0-" << n << ").";
        id %= n;
        LOG_TRACE << "Rounded to : " << id;
    }
#ifdef __linux__
    assert(listeningloopThreads_[id]);
    return listeningloopThreads_[id]->getLoop();
#else
    return ioLoopThreadPoolPtr_->getLoop(id);
#endif
}
void ListenerManager::stopListening()
{
    for (auto &serverPtr : servers_)
    {
        serverPtr->stop();
    }
    for (auto loop : ioLoops_)
    {
        assert(!loop->isInLoopThread());
        if (loop->isRunning())
        {
            std::promise<int> pro;
            auto f = pro.get_future();
            loop->queueInLoop([loop, &pro]() {
                loop->quit();
                pro.set_value(1);
            });
            (void)f.get();
        }
    }
#ifndef __linux__
    for (auto &listenerLoopPtr : listeningloopThreads_)
    {
        auto loop = listenerLoopPtr->getLoop();
        assert(!loop->isInLoopThread());
        if (loop->isRunning())
        {
            std::promise<int> pro;
            auto f = pro.get_future();
            loop->queueInLoop([loop, &pro]() {
                loop->quit();
                pro.set_value(1);
            });
            (void)f.get();
        }
    }
#endif
}

std::vector<trantor::InetAddress> ListenerManager::getListeners() const
{
    std::vector<trantor::InetAddress> listeners;
    for (auto &server : servers_)
    {
        listeners.emplace_back(server->address());
    }
    return listeners;
}
