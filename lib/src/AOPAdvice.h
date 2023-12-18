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
#include <trantor/net/InetAddress.h>
#include <functional>
#include <vector>

namespace drogon
{
class AopAdvice
{
  public:
    static AopAdvice &instance()
    {
        static AopAdvice inst;
        return inst;
    }

    // Getters?
    bool hasPreRoutingAdvices() const
    {
        return !preRoutingAdvices_.empty();
    }

    bool hasPostRoutingAdvices() const
    {
        return !postRoutingAdvices_.empty();
    }

    bool hasPreHandlingAdvices() const
    {
        return !preHandlingAdvices_.empty();
    }

    // Setters?
    void registerNewConnectionAdvice(
        std::function<bool(const trantor::InetAddress &,
                           const trantor::InetAddress &)> advice)
    {
        newConnectionAdvices_.emplace_back(std::move(advice));
    }

    void registerHttpResponseCreationAdvice(
        std::function<void(const HttpResponsePtr &)> advice)
    {
        responseCreationAdvices_.emplace_back(std::move(advice));
    }

    void registerSyncAdvice(
        std::function<HttpResponsePtr(const HttpRequestPtr &)> advice)

    {
        syncAdvices_.emplace_back(std::move(advice));
    }

    void registerPreRoutingObserver(
        std::function<void(const HttpRequestPtr &)> advice)
    {
        preRoutingObservers_.emplace_back(std::move(advice));
    }

    void registerPreRoutingAdvice(
        std::function<void(const HttpRequestPtr &,
                           AdviceCallback &&,
                           AdviceChainCallback &&)> advice)
    {
        preRoutingAdvices_.emplace_back(std::move(advice));
    }

    void registerPostRoutingObserver(
        std::function<void(const HttpRequestPtr &)> advice)
    {
        postRoutingObservers_.emplace_back(std::move(advice));
    }

    void registerPostRoutingAdvice(
        std::function<void(const HttpRequestPtr &,
                           AdviceCallback &&,
                           AdviceChainCallback &&)> advice)
    {
        postRoutingAdvices_.emplace_back(std::move(advice));
    }

    void registerPreHandlingObserver(
        std::function<void(const HttpRequestPtr &)> advice)
    {
        preHandlingObservers_.emplace_back(std::move(advice));
    }

    void registerPreHandlingAdvice(
        std::function<void(const HttpRequestPtr &,
                           AdviceCallback &&,
                           AdviceChainCallback &&)> advice)
    {
        preHandlingAdvices_.emplace_back(std::move(advice));
    }

    void registerPostHandlingAdvice(
        std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>
            advice)
    {
        postHandlingAdvices_.emplace_back(std::move(advice));
    }

    void registerPreSendingAdvice(
        std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>
            advice)
    {
        preSendingAdvices_.emplace_back(std::move(advice));
    }

    // Executors
    bool passNewConnectionAdvices(const trantor::TcpConnectionPtr &conn) const;
    void passResponseCreationAdvices(const HttpResponsePtr &resp) const;

    HttpResponsePtr passSyncAdvices(const HttpRequestPtr &req) const;
    void passPreRoutingObservers(const HttpRequestImplPtr &req) const;
    void passPreRoutingAdvices(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const;
    void passPostRoutingObservers(const HttpRequestImplPtr &req) const;
    void passPostRoutingAdvices(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const;
    void passPreHandlingObservers(const HttpRequestImplPtr &req) const;
    void passPreHandlingAdvices(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) const;
    void passPostHandlingAdvices(const HttpRequestImplPtr &req,
                                 const HttpResponsePtr &resp) const;
    void passPreSendingAdvices(const HttpRequestImplPtr &req,
                               const HttpResponsePtr &resp) const;

  private:
    using SyncAdvice = std::function<HttpResponsePtr(const HttpRequestPtr &)>;
    using SyncReqObserver = std::function<void(const HttpRequestPtr &)>;
    using SyncObserver =
        std::function<void(const HttpRequestPtr &, const HttpResponsePtr &)>;
    using AsyncAdvice = std::function<void(const HttpRequestPtr &,
                                           AdviceCallback &&,
                                           AdviceChainCallback &&)>;

    // If we want to add aop functions anytime, we can add a mutex here

    std::vector<std::function<bool(const trantor::InetAddress &,
                                   const trantor::InetAddress &)>>
        newConnectionAdvices_;
    std::vector<std::function<void(const HttpResponsePtr &)>>
        responseCreationAdvices_;

    std::vector<SyncAdvice> syncAdvices_;
    std::vector<SyncReqObserver> preRoutingObservers_;
    std::vector<AsyncAdvice> preRoutingAdvices_;
    std::vector<SyncReqObserver> postRoutingObservers_;
    std::vector<AsyncAdvice> postRoutingAdvices_;
    std::vector<SyncReqObserver> preHandlingObservers_;
    std::vector<AsyncAdvice> preHandlingAdvices_;
    std::vector<SyncObserver> postHandlingAdvices_;
    std::vector<SyncObserver> preSendingAdvices_;
};

}  // namespace drogon
