//
// Created by antao on 2018/6/22.
//
#include "PgClientImpl.h"
#include "PgConnection.h"
#include <trantor/net/EventLoop.h>
#include <trantor/net/inner/Channel.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/DbClient.h>
#include <libpq-fe.h>
#include <sys/select.h>
#include <iostream>
#include <thread>
#include <vector>
#include <unordered_set>
#include <memory>
#include <stdio.h>
#include <unistd.h>
#include <sstream>

using namespace drogon::orm;

DbConnectionPtr PgClientImpl::newConnection()
{
    //std::cout<<"newConn"<<std::endl;
    auto connPtr = std::make_shared<PgConnection>(_loopPtr.get(), _connInfo);
    auto loopPtr = _loopPtr;
    std::weak_ptr<PgClientImpl> weakPtr = shared_from_this();
    connPtr->setCloseCallback([weakPtr, loopPtr](const DbConnectionPtr &closeConnPtr) {
        //Reconnect after 1 second
        loopPtr->runAfter(1, [weakPtr, closeConnPtr] {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            std::lock_guard<std::mutex> guard(thisPtr->_connectionsMutex);
            thisPtr->_readyConnections.erase(closeConnPtr);
            thisPtr->_busyConnections.erase(closeConnPtr);
            assert(thisPtr->_connections.find(closeConnPtr) != thisPtr->_connections.end());
            thisPtr->_connections.erase(closeConnPtr);
            thisPtr->_connections.insert(thisPtr->newConnection());
        });
    });
    connPtr->setOkCallback([weakPtr](const DbConnectionPtr &okConnPtr) {
        LOG_TRACE << "postgreSQL connected!";
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        thisPtr->handleNewTask(okConnPtr);
    });
    //std::cout<<"newConn end"<<connPtr<<std::endl;
    return connPtr;
}
