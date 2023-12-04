/**
 *
 *  AOPAdvice.h
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
#include "impl_forwards.h"
#include <drogon/drogon_callbacks.h>
#include <deque>
#include <functional>
#include <vector>
#include <memory>

namespace drogon
{
class AopAdvice
{
  public:
    using ReqBlockerVector =
        std::vector<std::function<HttpResponsePtr(const HttpRequestPtr &)>>;
    using ReqObserverVector =
        std::vector<std::function<void(const HttpRequestPtr &)>>;
    using ReqRespObserverVector = std::vector<
        std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>>;
    using AsyncReqAdviceVector =
        std::vector<std::function<void(const HttpRequestPtr &,
                                       AdviceCallback &&,
                                       AdviceChainCallback &&)>>;

    static AopAdvice &instance()
    {
        static AopAdvice inst;
        return inst;
    }

    // Setters?
    void registerSyncAdvice(
        const std::function<HttpResponsePtr(const HttpRequestPtr &)> &advice)

    {
        syncAdvices_.emplace_back(advice);
    }

    void registerPreRoutingObserver(
        const std::function<void(const HttpRequestPtr &)> &advice)
    {
        preRoutingObservers_.emplace_back(advice);
    }

    void registerPreRoutingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 AdviceCallback &&,
                                 AdviceChainCallback &&)> &advice)
    {
        preRoutingAdvices_.emplace_back(advice);
    }

    void registerPostRoutingObserver(
        const std::function<void(const HttpRequestPtr &)> &advice)
    {
        postRoutingObservers_.emplace_back(advice);
    }

    void registerPostRoutingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 AdviceCallback &&,
                                 AdviceChainCallback &&)> &advice)
    {
        postRoutingAdvices_.emplace_back(advice);
    }

    void registerPreHandlingObserver(
        const std::function<void(const HttpRequestPtr &)> &advice)
    {
        preHandlingObservers_.emplace_back(advice);
    }

    void registerPreHandlingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 AdviceCallback &&,
                                 AdviceChainCallback &&)> &advice)
    {
        preHandlingAdvices_.emplace_back(advice);
    }

    void registerPostHandlingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 const HttpResponsePtr &)> &advice)
    {
        postHandlingAdvices_.emplace_back(advice);
    }

    void registerPreSendingAdvice(
        const std::function<void(const HttpRequestPtr &,
                                 const HttpResponsePtr &)> &advice)
    {
        preSendingAdvices_.emplace_back(advice);
    }

    // Executors
    HttpResponsePtr passSyncAdvices(const HttpRequestPtr &req);
    void passPreRoutingObservers(const HttpRequestImplPtr &req);
    void passPreRoutingAdvices(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback);
    void passPostRoutingObservers(const HttpRequestImplPtr &req);
    void passPostRoutingAdvices(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback);
    void passPreHandlingObservers(const HttpRequestImplPtr &req);
    void passPreHandlingAdvices(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback);
    void passPostHandlingAdvices(const HttpRequestImplPtr &req,
                                 const HttpResponsePtr &resp);
    void passPreSendingAdvices(const HttpRequestImplPtr &req,
                               const HttpResponsePtr &resp);

  private:
    // If we want to add aop functions anytime, we can add a mutex here
    ReqBlockerVector syncAdvices_;
    ReqObserverVector preRoutingObservers_;
    AsyncReqAdviceVector preRoutingAdvices_;
    ReqObserverVector postRoutingObservers_;
    AsyncReqAdviceVector postRoutingAdvices_;
    ReqObserverVector preHandlingObservers_;
    AsyncReqAdviceVector preHandlingAdvices_;
    ReqRespObserverVector postHandlingAdvices_;
    ReqRespObserverVector preSendingAdvices_;
};

}  // namespace drogon
