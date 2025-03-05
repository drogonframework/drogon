/**
 *
 *  AOPAdvice.cc
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

#include "AOPAdvice.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include <trantor/net/TcpConnection.h>

namespace drogon
{

static void doAdviceChain(
    const std::vector<std::function<void(const HttpRequestPtr &,
                                         AdviceCallback &&,
                                         AdviceChainCallback &&)>> &adviceChain,
    size_t index,
    const HttpRequestImplPtr &req,
    std::shared_ptr<const std::function<void(const HttpResponsePtr &)>>
        &&callbackPtr);

bool AopAdvice::passNewConnectionAdvices(
    const trantor::TcpConnectionPtr &conn) const
{
    for (auto &advice : newConnectionAdvices_)
    {
        if (!advice(conn->localAddr(), conn->peerAddr()))
        {
            return false;
        }
    }
    return true;
}

void AopAdvice::passResponseCreationAdvices(const HttpResponsePtr &resp) const
{
    if (!responseCreationAdvices_.empty())
    {
        for (auto &advice : responseCreationAdvices_)
        {
            advice(resp);
        }
    }
}

HttpResponsePtr AopAdvice::passSyncAdvices(const HttpRequestPtr &req) const
{
    for (auto &advice : syncAdvices_)
    {
        if (auto resp = advice(req))
        {
            return resp;
        }
    }
    return nullptr;
}

void AopAdvice::passPreRoutingObservers(const HttpRequestImplPtr &req) const
{
    if (!preRoutingObservers_.empty())
    {
        for (auto &observer : preRoutingObservers_)
        {
            observer(req);
        }
    }
}

void AopAdvice::passPreRoutingAdvices(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    if (preRoutingAdvices_.empty())
    {
        callback(nullptr);
        return;
    }

    auto callbackPtr =
        std::make_shared<std::decay_t<decltype(callback)>>(std::move(callback));
    doAdviceChain(preRoutingAdvices_, 0, req, std::move(callbackPtr));
}

void AopAdvice::passPostRoutingObservers(const HttpRequestImplPtr &req) const
{
    if (!postRoutingObservers_.empty())
    {
        for (auto &observer : postRoutingObservers_)
        {
            observer(req);
        }
    }
}

void AopAdvice::passPostRoutingAdvices(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    if (postRoutingAdvices_.empty())
    {
        callback(nullptr);
        return;
    }

    auto callbackPtr =
        std::make_shared<std::decay_t<decltype(callback)>>(std::move(callback));
    doAdviceChain(postRoutingAdvices_, 0, req, std::move(callbackPtr));
}

void AopAdvice::passPreHandlingObservers(const HttpRequestImplPtr &req) const
{
    if (!preHandlingObservers_.empty())
    {
        for (auto &observer : preHandlingObservers_)
        {
            observer(req);
        }
    }
}

void AopAdvice::passPreHandlingAdvices(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    if (preHandlingAdvices_.empty())
    {
        callback(nullptr);
        return;
    }

    auto callbackPtr =
        std::make_shared<std::decay_t<decltype(callback)>>(std::move(callback));
    doAdviceChain(preHandlingAdvices_, 0, req, std::move(callbackPtr));
}

void AopAdvice::passPostHandlingAdvices(const HttpRequestImplPtr &req,
                                        const HttpResponsePtr &resp) const
{
    for (auto &advice : postHandlingAdvices_)
    {
        advice(req, resp);
    }
}

void AopAdvice::passPreSendingAdvices(const HttpRequestImplPtr &req,
                                      const HttpResponsePtr &resp) const
{
    for (auto &advice : preSendingAdvices_)
    {
        advice(req, resp);
    }
}

static void doAdviceChain(
    const std::vector<std::function<void(const HttpRequestPtr &,
                                         AdviceCallback &&,
                                         AdviceChainCallback &&)>> &adviceChain,
    size_t index,
    const HttpRequestImplPtr &req,
    std::shared_ptr<const std::function<void(const HttpResponsePtr &)>>
        &&callbackPtr)
{
    if (index < adviceChain.size())
    {
        auto &advice = adviceChain[index];
        advice(
            req,
            [/*copy*/ callbackPtr](const HttpResponsePtr &resp) {
                (*callbackPtr)(resp);
            },
            [index, req, callbackPtr, &adviceChain]() mutable {
                auto ioLoop = req->getLoop();
                if (ioLoop && !ioLoop->isInLoopThread())
                {
                    ioLoop->queueInLoop([index,
                                         req,
                                         callbackPtr = std::move(callbackPtr),
                                         &adviceChain]() mutable {
                        doAdviceChain(adviceChain,
                                      index + 1,
                                      req,
                                      std::move(callbackPtr));
                    });
                }
                else
                {
                    doAdviceChain(adviceChain,
                                  index + 1,
                                  req,
                                  std::move(callbackPtr));
                }
            });
    }
    else
    {
        (*callbackPtr)(nullptr);
    }
}

}  // namespace drogon
