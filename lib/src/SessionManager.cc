/**
 *
 *  SessionManager.cc
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
#include <drogon/utils/Utilities.h>

using namespace drogon;

SessionManager::SessionManager(trantor::EventLoop *loop, size_t timeout)
    : loop_(loop), timeout_(timeout)
{
    assert(timeout_ >= 0);
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
                loop_, 1.0, wheelNum, bucketNum));
    }
    else if (timeout_ == 0)
    {
        sessionMapPtr_ = std::unique_ptr<CacheMap<std::string, SessionPtr>>(
            new CacheMap<std::string, SessionPtr>(loop_, 0, 0, 0));
    }
}

SessionPtr SessionManager::getSession(const std::string &sessionID,
                                      bool needToSet)
{
    assert(!sessionID.empty());
    SessionPtr sessionPtr;
    std::lock_guard<std::mutex> lock(mapMutex_);
    if (sessionMapPtr_->findAndFetch(sessionID, sessionPtr) == false)
    {
        sessionPtr =
            std::shared_ptr<Session>(new Session(sessionID, needToSet));
        sessionMapPtr_->insert(sessionID, sessionPtr, timeout_);
        return sessionPtr;
    }
    return sessionPtr;
}

void SessionManager::changeSessionId(const SessionPtr &sessionPtr)
{
    auto oldId = sessionPtr->sessionId();
    auto newId = utils::getUuid();
    sessionPtr->setSessionId(newId);
    {
        std::lock_guard<std::mutex> lock(mapMutex_);
        // For requests sent before setting the new session ID to the client, we
        // reserve the old session slot for a period of time.
        sessionMapPtr_->runAfter(10, [this, oldId = std::move(oldId)]() {
            LOG_TRACE << "remove the old slot of the session";
            sessionMapPtr_->erase(oldId);
        });
        sessionMapPtr_->insert(newId, sessionPtr, timeout_);
    }
}