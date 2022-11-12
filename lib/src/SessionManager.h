/**
 *
 *  @file SessionManager.h
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
#include <drogon/drogon_callbacks.h>
#include <drogon/CacheMap.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoop.h>
#include <memory>
#include <string>
#include <mutex>

namespace drogon
{
class SessionManager : public trantor::NonCopyable
{
  public:
    SessionManager(trantor::EventLoop *loop,
                   size_t timeout,
                   AdviceStartSessionCallback startAdvice = { [](const std::string &){} },
                   AdviceDestroySessionCallback destroyAdvice = { [](const std::string &){} }
                  );
    ~SessionManager()
    {
        sessionMapPtr_.reset();
    }
    SessionPtr getSession(const std::string &sessionID, bool needToSet);
    void changeSessionId(const SessionPtr &sessionPtr);

    void registerSessionAdvices( 
                                AdviceStartSessionCallback startAdvice,
                                AdviceDestroySessionCallback destroyAdvice
                               )
    {
        sessionStartAdviceHandler_ = startAdvice;
        sessionDestroyAdviceHandler_ = destroyAdvice;
    }

  private:
    std::unique_ptr<CacheMap<std::string, SessionPtr>> sessionMapPtr_;
    trantor::EventLoop *loop_;
    size_t timeout_;
    AdviceStartSessionCallback   sessionStartAdviceHandler_;
    AdviceDestroySessionCallback sessionDestroyAdviceHandler_;
};
}  // namespace drogon
