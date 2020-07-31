/**
 *
 *  SessionManager.h
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

#include <drogon/Session.h>
#include <drogon/CacheMap.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoop.h>
#include <memory>
#include <string>
#include <mutex>

namespace drogon
{
std::unique_ptr<SessionStorageProvider> createCacheMapProvider(
    trantor::EventLoop *loop,
    size_t timeout);

class SessionManager : public trantor::NonCopyable
{
  public:
    SessionManager(std::unique_ptr<SessionStorageProvider> provider,
                   size_t timeout);
    ~SessionManager()
    {
        provider_.reset();
    }
    SessionPtr getSession(const std::string &sessionID, bool needToSet);

  private:
    std::unique_ptr<SessionStorageProvider> provider_;
    std::mutex mapMutex_;
    size_t timeout_;
};
}  // namespace drogon
