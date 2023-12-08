//
// Created by wanchen.he on 2023/12/8.
//

#include "HttpConnectionLimit.h"
#include "AOPAdvice.h"
#include <trantor/utils/Logger.h>

using namespace drogon;

void HttpConnectionLimit::setMaxConnectionNum(size_t num)
{
    maxConnectionNum_ = num;
}

void HttpConnectionLimit::setMaxConnectionNumPerIP(size_t num)
{
    maxConnectionNumPerIP_ = num;
}

void HttpConnectionLimit::tryAddConnection(
    const trantor::TcpConnectionPtr &conn)
{
    assert(conn->connected());
    if (connectionNum_.fetch_add(1, std::memory_order_relaxed) >=
        maxConnectionNum_)
    {
        LOG_ERROR << "too much connections!force close!";
        conn->forceClose();
        return;
    }
    if (maxConnectionNumPerIP_ > 0)
    {
        std::string ip = conn->peerAddr().toIp();
        size_t numOnThisIp;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            numOnThisIp = (++ipConnectionsMap_[ip]);
        }
        if (numOnThisIp > maxConnectionNumPerIP_)
        {
            conn->forceClose();
            return;
        }
    }

    if (!AopAdvice::instance().passNewConnectionAdvices(conn))
    {
        conn->forceClose();
    }
}

void HttpConnectionLimit::releaseConnection(
    const trantor::TcpConnectionPtr &conn)
{
    assert(!conn->connected());
    if (!conn->hasContext())
    {
        // If the connection is connected to the SSL port and then
        // disconnected before the SSL handshake.
        return;
    }
    connectionNum_.fetch_sub(1, std::memory_order_relaxed);
    if (maxConnectionNumPerIP_ > 0)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto iter = ipConnectionsMap_.find(conn->peerAddr().toIp());
        if (iter != ipConnectionsMap_.end())
        {
            --iter->second;
            if (iter->second <= 0)
            {
                ipConnectionsMap_.erase(iter);
            }
        }
    }
}
