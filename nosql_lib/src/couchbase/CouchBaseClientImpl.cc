/**
 *
 *  @file CouchBaseClientImpl.cc
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

#include "CouchBaseClientImpl.h"
#include "CouchBaseConnection.h"
#include "CouchBaseCommand.h"
#include <trantor/utils/Logger.h>

using namespace drogon::nosql;
using namespace std::chrono_literals;

CouchBaseClientImpl::CouchBaseClientImpl(const std::string &connectString,
                                         const std::string &userName,
                                         const std::string &password,
                                         size_t connNum)
    : connectString_(connectString),
      userName_(userName),
      password_(password),
      connectionsNumber_(connNum),
      loops_(connNum < std::thread::hardware_concurrency()
                 ? connNum
                 : std::thread::hardware_concurrency(),
             "CouchBaseLoop")
{
    assert(connNum > 0);
    loops_.start();
    std::thread([this]() {
        for (size_t i = 0; i < connectionsNumber_; ++i)
        {
            auto loop = loops_.getNextLoop();
            loop->queueInLoop([this, loop]() {
                std::lock_guard<std::mutex> lock(connectionsMutex_);
                connectionsPool_.addNewConnection(newConnection(loop));
            });
        }
    }).detach();
}

CouchBaseConnectionPtr CouchBaseClientImpl::newConnection(
    trantor::EventLoop *loop)
{
    auto connPtr = std::make_shared<CouchBaseConnection>(connectString_,
                                                         userName_,
                                                         password_,
                                                         loop);
    std::weak_ptr<CouchBaseClientImpl> weakPtr = shared_from_this();
    connPtr->setCloseCallback(
        [weakPtr](const CouchBaseConnectionPtr &closeConnPtr) {
            LOG_ERROR << "disconnected!";
            // Erase the connection
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            {
                std::lock_guard<std::mutex> guard(thisPtr->connectionsMutex_);
                thisPtr->connectionsPool_.removeConnection(closeConnPtr);
            }
            // Reconnect after 1 second
            auto loop = closeConnPtr->loop();
            loop->runAfter(1s, [weakPtr, loop] {
                auto thisPtr = weakPtr.lock();
                if (!thisPtr)
                    return;
                std::lock_guard<std::mutex> guard(thisPtr->connectionsMutex_);
                thisPtr->connectionsPool_.addNewConnection(
                    thisPtr->newConnection(loop));
            });
        });
    connPtr->setOkCallback([weakPtr](const CouchBaseConnectionPtr &okConnPtr) {
        LOG_TRACE << "connected!";
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        {
            std::lock_guard<std::mutex> guard(thisPtr->connectionsMutex_);
            thisPtr->connectionsPool_.addBusyConnection(
                okConnPtr);  // For new connections, this sentence is necessary
        }
        thisPtr->handleNewTask(okConnPtr);
    });
    std::weak_ptr<CouchBaseConnection> weakConn = connPtr;
    connPtr->setIdleCallback([weakPtr, weakConn]() {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        auto connPtr = weakConn.lock();
        if (!connPtr)
            return;
        thisPtr->handleNewTask(connPtr);
    });
    // std::cout<<"newConn end"<<connPtr<<std::endl;
    return connPtr;
}

void CouchBaseClientImpl::handleNewTask(const CouchBaseConnectionPtr &connPtr)
{
    CouchBaseCommandPtr cmd;
    {
        std::lock_guard<std::mutex> guard(connectionsMutex_);
        if (connectionsPool_.hasCommand())
        {
            cmd = connectionsPool_.getCommand();
        }
        else
        {
            // Connection is idle, put it into the readyConnections_ set;
            connectionsPool_.setConnectionIdle(connPtr);
        }
    }
    if (cmd)
    {
        switch (cmd->type())
        {
            case CommandType::kGet:
            {
                auto getCommand = reinterpret_cast<GetCommand *>(cmd.get());
                connPtr->get(getCommand->key_,
                             std::move(getCommand->callback_),
                             std::move(getCommand->errorCallback_));
            }
            break;
            case CommandType::kStore:
            {
                auto storeCommand = reinterpret_cast<StoreCommand *>(cmd.get());
                connPtr->store(storeCommand->key_,
                               storeCommand->value_,
                               std::move(storeCommand->callback_),
                               std::move(storeCommand->errorCallback_));
            }
            break;
            default:
                break;
        }
    }
}

void CouchBaseClientImpl::get(const std::string &key,
                              CBCallback &&callback,
                              ExceptionCallback &&errorCallback)
{
    std::pair<bool, CouchBaseConnectionPtr> r;
    {
        std::lock_guard<std::mutex> guard(connectionsMutex_);
        r = connectionsPool_.getIdleConnection();
        if (r.first && !r.second)
        {
            auto cmd = std::make_shared<GetCommand>(key,
                                                    std::move(callback),
                                                    std::move(errorCallback));
            connectionsPool_.putCommandToBuffer(std::move(cmd));
        }
    }
    if (r.second)
    {
        r.second->get(key, std::move(callback), std::move(errorCallback));
        return;
    }
    if (!r.first)
    {
        // too many queries in buffer;
        // TODO exceptCallback
        return;
    }
}
void CouchBaseClientImpl::store(const std::string &key,
                                const std::string &value,
                                CBCallback &&callback,
                                ExceptionCallback &&errorCallback)
{
    std::pair<bool, CouchBaseConnectionPtr> r;
    {
        std::lock_guard<std::mutex> guard(connectionsMutex_);
        r = connectionsPool_.getIdleConnection();
        if (r.first && !r.second)
        {
            auto cmd = std::make_shared<StoreCommand>(key,
                                                      value,
                                                      std::move(callback),
                                                      std::move(errorCallback));
            connectionsPool_.putCommandToBuffer(std::move(cmd));
        }
    }
    if (r.second)
    {
        r.second->store(key,
                        value,
                        std::move(callback),
                        std::move(errorCallback));
        return;
    }
    if (!r.first)
    {
        // too many queries in buffer;
        // TODO exceptCallback
        return;
    }
}

std::shared_ptr<CouchBaseClient> CouchBaseClient::newClient(
    const std::string &connectString,
    const std::string &userName,
    const std::string &password,
    size_t connNum)
{
    return std::make_shared<CouchBaseClientImpl>(connectString,
                                                 userName,
                                                 password,
                                                 connNum);
}