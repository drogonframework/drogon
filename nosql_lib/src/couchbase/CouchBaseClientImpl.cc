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
                connections_.insert(newConnection(loop));
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
                thisPtr->readyConnections_.erase(closeConnPtr);
                thisPtr->busyConnections_.erase(closeConnPtr);
                assert(thisPtr->connections_.find(closeConnPtr) !=
                       thisPtr->connections_.end());
                thisPtr->connections_.erase(closeConnPtr);
            }
            // Reconnect after 1 second
            auto loop = closeConnPtr->loop();
            loop->runAfter(1s, [weakPtr, loop] {
                auto thisPtr = weakPtr.lock();
                if (!thisPtr)
                    return;
                std::lock_guard<std::mutex> guard(thisPtr->connectionsMutex_);
                thisPtr->connections_.insert(thisPtr->newConnection(loop));
            });
        });
    connPtr->setOkCallback([weakPtr](const CouchBaseConnectionPtr &okConnPtr) {
        LOG_TRACE << "connected!";
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        {
            std::lock_guard<std::mutex> guard(thisPtr->connectionsMutex_);
            thisPtr->busyConnections_.insert(
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
        if (!commandsBuffer_.empty())
        {
            cmd = std::move(commandsBuffer_.front());
            commandsBuffer_.pop_front();
        }
        else
        {
            // Connection is idle, put it into the readyConnections_ set;
            busyConnections_.erase(connPtr);
            readyConnections_.insert(connPtr);
        }
    }
    if (cmd)
    {
        switch (cmd->type_)
        {
            case CommandType::kGet:
                connPtr->get(cmd->key_,
                             std::move(cmd->callback_),
                             std::move(cmd->errorCallback_));
                break;
            case CommandType::kStore:
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
    bool busy{false};
    CouchBaseConnectionPtr conn;
    {
        std::lock_guard<std::mutex> guard(connectionsMutex_);
        if (readyConnections_.size() == 0)
        {
            if (commandsBuffer_.size() > 200000)
            {
                // too many queries in buffer;
                busy = true;
            }
            else
            {
                // LOG_TRACE << "Push query to buffer";
                // TODO: make command
                auto cmd = std::make_shared<CouchBaseCommand>();
                cmd->key_ = key;
                cmd->type_ = CommandType::kGet;
                cmd->callback_ = std::move(callback);
                cmd->errorCallback_ = std::move(errorCallback);
                commandsBuffer_.push_back(std::move(cmd));
            }
        }
        else
        {
            auto iter = readyConnections_.begin();
            busyConnections_.insert(*iter);
            conn = *iter;
            readyConnections_.erase(iter);
        }
    }
    if (conn)
    {
        conn->get(key, std::move(callback), std::move(errorCallback));
        return;
    }
    if (busy)
    {
        // TODO exceptCallback
        return;
    }
}

CouchBaseConnectionPtr CouchBaseClientImpl::getIdleConnection()
{
    std::lock_guard<std::mutex> guard(connectionsMutex_);
    return nullptr;
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