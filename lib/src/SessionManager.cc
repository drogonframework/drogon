/**
 *
 *  @file SessionManager.cc
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

#include "SessionManager.h"

using namespace drogon;

SessionManager::SessionManager(
    trantor::EventLoop *loop,
    size_t timeout,
    const std::vector<AdviceStartSessionCallback> &startAdvices,
    const std::vector<AdviceDestroySessionCallback> &destroyAdvices,
    IdGeneratorCallback idGeneratorCallback)
    : loop_(loop),
      timeout_(timeout),
      sessionStartAdvices_(startAdvices),
      sessionDestroyAdvices_(destroyAdvices),
      idGeneratorCallback_(idGeneratorCallback)
{
    if (timeout_ > 0)
    {
        size_t wheelNum = 1;
        size_t bucketNum = 0;
        if (timeout_ < 500)
        {
            bucketNum = timeout_ + 1;
        }
        else
        {
            auto tmpTimeout = timeout_;
            bucketNum = 100;
            while (tmpTimeout > 100)
            {
                ++wheelNum;
                tmpTimeout = tmpTimeout / 100;
            }
        }

        sessionMapPtr_ = std::unique_ptr<CacheMap<std::string, SessionPtr>>(
            new CacheMap<std::string, SessionPtr>(
                loop_,
                1.0,
                wheelNum,
                bucketNum,
                [this](const std::string &key) {
                    for (auto &advice : sessionStartAdvices_)
                    {
                        advice(key);
                    }
                },
                [this](const std::string &key) {
                    for (auto &advice : sessionDestroyAdvices_)
                    {
                        advice(key);
                    }
                }));
    }
    else if (timeout_ == 0)
    {
        sessionMapPtr_ = std::unique_ptr<CacheMap<std::string, SessionPtr>>(
            new CacheMap<std::string, SessionPtr>(
                loop_,
                0,
                0,
                0,
                [this](const std::string &key) {
                    for (auto &advice : sessionStartAdvices_)
                    {
                        advice(key);
                    }
                },
                [this](const std::string &key) {
                    for (auto &advice : sessionDestroyAdvices_)
                    {
                        advice(key);
                    }
                }));
    }
}

SessionPtr SessionManager::getSession(const std::string &sessionID,
                                      bool needToSet)
{
    assert(!sessionID.empty());
    SessionPtr sessionPtr;
    sessionMapPtr_->modify(
        sessionID,
        [&sessionPtr, &sessionID, needToSet](SessionPtr &sessionInCache) {
            if (sessionInCache)
            {
                sessionPtr = sessionInCache;
            }
            else
            {
                sessionPtr =
                    std::shared_ptr<Session>(new Session(sessionID, needToSet));
                sessionInCache = sessionPtr;
            }
        },
        timeout_);

    return sessionPtr;
}

void SessionManager::changeSessionId(const SessionPtr &sessionPtr)
{
    auto newId = idGeneratorCallback_();
    std::string oldId;
    {
        // A session may be handled concurrently by multiple IO loops. Claim
        // the requested rotation and publish the new map entry as one session
        // state transition so only one response performs it.
        std::lock_guard<std::mutex> lock(sessionPtr->mutex_);
        if (!sessionPtr->needToChange_)
            return;
        oldId = sessionPtr->sessionId_;
        sessionPtr->sessionId_ = newId;
        sessionPtr->needToChange_ = false;
        sessionMapPtr_->insert(newId, sessionPtr, timeout_);
    }
    // For requests sent before setting the new session ID to the client, we
    // reserve the old session slot for a period of time.
    sessionMapPtr_->runAfter(10, [this, oldId = std::move(oldId)]() {
        LOG_TRACE << "remove the old slot of the session";
        sessionMapPtr_->erase(oldId);
    });
}
