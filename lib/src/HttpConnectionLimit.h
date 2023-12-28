/**
 *
 *  @file HttpConnectionLimit.h
 *  @author Nitromelon
 *
 *  Copyright 2023, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <string>
#include <unordered_map>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <trantor/net/TcpConnection.h>

namespace drogon
{
class HttpConnectionLimit
{
  public:
    static HttpConnectionLimit &instance()
    {
        static HttpConnectionLimit inst;
        return inst;
    }

    size_t getConnectionNum() const
    {
        return connectionNum_.load(std::memory_order_relaxed);
    }

    // don't set after start
    void setMaxConnectionNum(size_t num);
    void setMaxConnectionNumPerIP(size_t num);

    bool tryAddConnection(const trantor::TcpConnectionPtr &conn);
    void releaseConnection(const trantor::TcpConnectionPtr &conn);

  private:
    std::mutex mutex_;

    size_t maxConnectionNum_{100000};
    std::atomic<size_t> connectionNum_{0};

    size_t maxConnectionNumPerIP_{0};
    std::unordered_map<std::string, size_t> ipConnectionsMap_;
};
}  // namespace drogon
