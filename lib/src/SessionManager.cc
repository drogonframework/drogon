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

SessionManager::SessionManager(trantor::EventLoop *loop, size_t timeout)
    : _loop(loop), _timeout(timeout)
{
    assert(_timeout >= 0);
    if (_timeout > 0)
    {
        size_t wheelNum = 1;
        size_t bucketNum = 0;
        if (_timeout < 500)
        {
            bucketNum = _timeout + 1;
        }
        else
        {
            auto tmpTimeout = _timeout;
            bucketNum = 100;
            while (tmpTimeout > 100)
            {
                wheelNum++;
                tmpTimeout = tmpTimeout / 100;
            }
        }
        _sessionMapPtr = std::unique_ptr<CacheMap<std::string, SessionPtr>>(
            new CacheMap<std::string, SessionPtr>(
                _loop, 1.0, wheelNum, bucketNum));
    }
    else if (_timeout == 0)
    {
        _sessionMapPtr = std::unique_ptr<CacheMap<std::string, SessionPtr>>(
            new CacheMap<std::string, SessionPtr>(_loop, 0, 0, 0));
    }
}

SessionPtr SessionManager::getSession(const std::string &sessionID,
                                      bool needToSet)
{
    assert(!sessionID.empty());
    SessionPtr sessionPtr;
    std::lock_guard<std::mutex> lock(_mapMutex);
    if (_sessionMapPtr->findAndFetch(sessionID, sessionPtr) == false)
    {
        sessionPtr = std::make_shared<Session>(sessionID, needToSet);
        _sessionMapPtr->insert(sessionID, sessionPtr, _timeout);
        return sessionPtr;
    }
    return sessionPtr;
}