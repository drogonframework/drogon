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

using namespace drogon;

class CacheMapSessionStorageProvider : public SessionStorageProvider
{
  public:
    CacheMapSessionStorageProvider(trantor::EventLoop *loop,
                                   float tickInterval,
                                   size_t wheelsNum,
                                   size_t bucketsNumPerWheel)
        : data_(std::make_unique<CacheMap<std::string, SessionPtr>>(
              loop,
              tickInterval,
              wheelsNum,
              bucketsNumPerWheel))
    {
    }

    bool findAndFetch(const std::string &key, SessionPtr &value) override
    {
        return data_->findAndFetch(key, value);
    }

    void insert(const std::string &key,
                SessionPtr &&value,
                size_t timeout) override
    {
        data_->insert(key, std::forward<SessionPtr>(value), timeout);
    }

  private:
    std::unique_ptr<CacheMap<std::string, SessionPtr>> data_;
};

SessionManager::SessionManager(std::unique_ptr<SessionStorageProvider> provider,
                               size_t timeout)
    : provider_(std::move(provider)), timeout_(timeout)
{
}

SessionPtr SessionManager::getSession(const std::string &sessionID,
                                      bool needToSet)
{
    assert(!sessionID.empty());
    SessionPtr sessionPtr;
    std::lock_guard<std::mutex> lock(mapMutex_);
    if (!provider_->findAndFetch(sessionID, sessionPtr))
    {
        sessionPtr = std::make_shared<Session>(sessionID, needToSet);
        provider_->insert(sessionID, std::move(sessionPtr), timeout_);
        return sessionPtr;
    }
    return sessionPtr;
}

SessionStorageProvider::~SessionStorageProvider() = default;

std::unique_ptr<SessionStorageProvider> drogon::createCacheMapProvider(
    trantor::EventLoop *loop,
    size_t timeout)
{
    assert(timeout >= 0);
    if (timeout > 0)
    {
        size_t wheelNum = 1;
        size_t bucketNum = 0;
        if (timeout < 500)
        {
            bucketNum = timeout + 1;
        }
        else
        {
            auto tmpTimeout = timeout;
            bucketNum = 100;
            while (tmpTimeout > 100)
            {
                ++wheelNum;
                tmpTimeout = tmpTimeout / 100;
            }
        }
        return std::make_unique<CacheMapSessionStorageProvider>(loop,
                                                                1.0,
                                                                wheelNum,
                                                                bucketNum);
    }
    else if (timeout == 0)
    {
        return std::make_unique<CacheMapSessionStorageProvider>(loop, 0, 0, 0);
    }

    return nullptr;
}
