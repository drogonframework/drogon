/**
 *
 *  @file Http11ClientPool.h
 *  @author fantasy-peak
 *
 *  Copyright 2024, fantasy-peak.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once

#include <cassert>
#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <utility>

#include <trantor/utils/Logger.h>
#include <trantor/net/EventLoopThreadPool.h>
#ifdef __cpp_impl_coroutine
#include <drogon/utils/coroutine.h>
#endif
#include <drogon/drogon.h>

namespace drogon
{

struct Http11ClientPoolConfig
{
    std::string hostString;
    bool useOldTLS{false};
    bool validateCert{false};
    std::size_t size{100};
    std::function<void(HttpClientPtr &)> setCallback;
    std::size_t numOfThreads{std::thread::hardware_concurrency()};
    std::optional<std::size_t> keepaliveRequests;
    std::optional<std::chrono::seconds> idleTimeout;
    std::optional<std::chrono::seconds> maxLifeTime;
    std::optional<std::chrono::seconds> checkInterval;
};

class Http11ClientPool final
{
  public:
    Http11ClientPool(
        Http11ClientPoolConfig cfg,
        std::shared_ptr<trantor::EventLoopThreadPool> loopPool = nullptr)
        : cfg_(std::move(cfg)), loopPool_(std::move(loopPool))
    {
        if (loopPool_ == nullptr)
        {
            loopPool_ = std::make_shared<trantor::EventLoopThreadPool>(
                cfg_.numOfThreads);
            loopPool_->start();
            isSelfThreadPool_ = true;
        }
        else
        {
            isSelfThreadPool_ = false;
        }
        loopPtr_ = loopPool_->getNextLoop();

        for (std::size_t i = 0; i < cfg_.size; i++)
        {
            auto loopPtr = loopPool_->getNextLoop();
            auto func = [this, loopPtr]() mutable {
                auto client = HttpClient::newHttpClient(cfg_.hostString,
                                                        loopPtr,
                                                        cfg_.useOldTLS,
                                                        cfg_.validateCert);
                if (cfg_.setCallback)
                    cfg_.setCallback(client);
                return client;
            };
            httpClients_.emplace(std::make_shared<Connection>(func, cfg_));
        }
        LOG_DEBUG << "httpClients_ size:" << httpClients_.size();

        if (cfg_.idleTimeout.has_value() && cfg_.checkInterval.has_value())
        {
            timerId_ = loopPtr_->runEvery(cfg_.checkInterval.value(), [this] {
                std::unique_lock<std::mutex> lock(mutex_);
                if (httpClients_.empty())
                    return;
                std::queue<std::shared_ptr<Connection>> clients;
                while (!httpClients_.empty())
                {
                    auto connPtr = std::move(httpClients_.front());
                    httpClients_.pop();
                    if (connPtr->reachIdleTimeout())
                    {
                        // close tcp connection
                        connPtr->resetClientPtr();
                    }
                    clients.emplace(std::move(connPtr));
                }
                httpClients_ = std::move(clients);
            });
        }
    }

    ~Http11ClientPool()
    {
        if (timerId_)
        {
            std::promise<void> done;
            loopPtr_->runInLoop([&] {
                loopPtr_->invalidateTimer(timerId_.value());
                done.set_value();
            });
            done.get_future().wait();
        }
        if (isSelfThreadPool_)
        {
            for (auto &ptr : loopPool_->getLoops())
                ptr->runInLoop([=] { ptr->quit(); });
            loopPool_->wait();
            loopPool_.reset();
        }
        std::unique_lock<std::mutex> lock(mutex_);
        std::queue<std::shared_ptr<Connection>> tmp;
        httpClients_.swap(tmp);
    }

    Http11ClientPool(const Http11ClientPool &) = delete;
    Http11ClientPool &operator=(const Http11ClientPool &) = delete;
    Http11ClientPool(Http11ClientPool &&) = delete;
    Http11ClientPool &operator=(Http11ClientPool &&) = delete;

    void sendRequest(const HttpRequestPtr &req,
                     std::function<void(ReqResult, const HttpResponsePtr &)> cb,
                     double timeout = 0)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (httpClients_.empty())
        {
            httpRequest_.emplace(req, std::move(cb));
        }
        else
        {
            auto connPtr = std::move(httpClients_.front());
            httpClients_.pop();
            lock.unlock();
            send(std::move(connPtr), req, std::move(cb), timeout);
        }
        return;
    }

#ifdef __cpp_impl_coroutine

    auto sendRequestCoro(HttpRequestPtr req, double timeout = 0)
    {
        struct Awaiter
            : public CallbackAwaiter<std::tuple<ReqResult, HttpResponsePtr>>
        {
            Awaiter(Http11ClientPool *pool, HttpRequestPtr req, double timeout)
                : pool_(pool), req_(std::move(req)), timeout_(timeout)
            {
            }

            void await_suspend(std::coroutine_handle<> handle)
            {
                pool_->sendRequest(
                    req_,
                    [this, handle](ReqResult result,
                                   const HttpResponsePtr &ptr) {
                        setValue(std::make_tuple(result, ptr));
                        handle.resume();
                    },
                    timeout_);
            }

          private:
            Http11ClientPool *pool_;
            HttpRequestPtr req_;
            double timeout_;
        };

        return Awaiter{this, std::move(req), timeout};
    }

#endif

  private:
    struct Connection
    {
        Connection(std::function<HttpClientPtr()> cb,
                   const Http11ClientPoolConfig &cfg)
            : createHttpClientFunc_(std::move(cb)), cfg_(cfg)
        {
            init();
        }

        void init()
        {
            clientPtr_ = createHttpClientFunc_();
            auto now = std::chrono::system_clock::now();
            timePoint_ = now;
            startTimePoint_ = now;
            counter_ = 0;
        }

        void send(const HttpRequestPtr &req,
                  std::function<void(ReqResult, const HttpResponsePtr &)> cb,
                  double timeout)
        {
            if (isInvalid())
            {
                init();
            }
            assert(clientPtr_ != nullptr);
            clientPtr_->sendRequest(req, std::move(cb), timeout);
            ++counter_;
            auto now = std::chrono::system_clock::now();
            timePoint_ = now;
        }

        bool isInvalid()
        {
            auto now = std::chrono::system_clock::now();
            auto idleDut = now - timePoint_;
            auto dut = now - startTimePoint_;
            if ((clientPtr_ == nullptr) ||
                (cfg_.keepaliveRequests.has_value() &&
                 counter_ >= cfg_.keepaliveRequests.value()) ||
                (cfg_.idleTimeout.has_value() &&
                 idleDut >= cfg_.idleTimeout.value()) ||
                (cfg_.maxLifeTime.has_value() &&
                 dut >= cfg_.maxLifeTime.value()))
            {
                return true;
            }
            return false;
        }

        bool reachIdleTimeout()
        {
            auto now = std::chrono::system_clock::now();
            auto idleDut = now - timePoint_;
            if (idleDut >= cfg_.idleTimeout.value())
            {
                return true;
            }
            return false;
        }

        void resetClientPtr()
        {
            clientPtr_ = nullptr;
        }

        Http11ClientPoolConfig cfg_;
        std::function<HttpClientPtr()> createHttpClientFunc_;
        HttpClientPtr clientPtr_;
        std::chrono::time_point<std::chrono::system_clock> timePoint_;
        std::chrono::time_point<std::chrono::system_clock> startTimePoint_;
        std::size_t counter_{0};
    };

    void send(std::shared_ptr<Connection> connPtr,
              const HttpRequestPtr &req,
              std::function<void(ReqResult, const HttpResponsePtr &)> cb,
              double timeout)
    {
        connPtr->send(
            req,
            [connPtr, this, cb = std::move(cb), timeout](
                ReqResult result, const HttpResponsePtr &ptr) mutable {
                cb(result, ptr);
                std::unique_lock<std::mutex> lock(mutex_);
                if (httpRequest_.empty())
                {
                    httpClients_.emplace(std::move(connPtr));
                }
                else
                {
                    auto op = std::move(httpRequest_.front());
                    httpRequest_.pop();
                    lock.unlock();
                    auto &[req, cb] = op;
                    send(std::move(connPtr), req, std::move(cb), timeout);
                }
                return;
            },
            timeout);
    }

    Http11ClientPoolConfig cfg_;
    std::shared_ptr<trantor::EventLoopThreadPool> loopPool_;
    bool isSelfThreadPool_;
    trantor::EventLoop *loopPtr_;
    std::mutex mutex_;
    std::queue<std::shared_ptr<Connection>> httpClients_;
    std::queue<
        std::tuple<HttpRequestPtr,
                   std::function<void(ReqResult, const HttpResponsePtr &)>>>
        httpRequest_;
    std::optional<trantor::TimerId> timerId_;
};

}  // namespace drogon
