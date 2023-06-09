/**
 *
 *  Histogram.h
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
#include <drogon/utils/monitoring/Metric.h>
#include <drogon/utils/string_view.h>
#include <trantor/net/EventLoopThread.h>
#include <atomic>
#include <mutex>

namespace drogon
{
namespace monitoring
{
/**
 * This class is used to collect samples for a counter metric.
 * */
template <typename T>
class Histogram : public Metric
{
  public:
    using TimeBucket = T;
    Histogram(const std::string &name,
              const std::vector<std::string> &labelNames,
              const std::string &labelValues,
              const std::chrono::duration<double> &maxAge,
              uint64_t timeBucketsCount,
              trantor::EventLoop *loop = nullptr) noexcept(false)
        : Metric(name, labelNames, labelValues),
          timeBucketCount_(timeBucketsCount)
    {
        if (loop == nullptr)
        {
            loopThreadPtr_ = std::make_unique<trantor::EventLoopThread>();
            loopPtr_ = loopThreadPtr_->getLoop();
            loopThreadPtr_->run();
        }
        else
        {
            loopPtr_ = loop;
        }
        if (maxAge > std::chrono::seconds(0))
        {
            if (timeBucketsCount == 0)
            {
                throw std::runtime_error(
                    "timeBucketsCount must be greater than 0");
            }
            timerId_ = loopPtr_->runEvery(maxAge / timeBucketsCount,
                                          [this]() { rotateTimeBuckets(); });
        }
    }
    virtual void observe(double value) = 0;
    ~Histogram() override
    {
        if (timerId_ != trantor::InvalidTimerId)
        {
            loopPtr_->invalidateTimer(timerId_);
        }
    }

  protected:
    double sum_{0};
    uint64_t count_{0};
    virtual void observe(double value, TimeBucket &timeBucket) = 0;
    virtual T newTimeBucket() = 0;

  private:
    std::deque<TimeBucket> timeBuckets_;
    std::unique_ptr<trantor::EventLoopThread> loopThreadPtr_;
    trantor::EventLoop *loopPtr_{nullptr};
    std::mutex mutex_;
    trantor::TimerId timerId_{trantor::InvalidTimerId};
    size_t timeBucketCount_{0};
    void rotateTimeBuckets()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        timeBuckets_.emplace_back(newTimeBucket());
        if (timeBuckets_.size() > timeBucketCount_)
        {
            auto expiredTimeBucket = timeBuckets_.front();
            sum_ -= expiredTimeBucket.sum_;
            count_ -= expiredTimeBucket.count_;
            timeBuckets_.erase(timeBuckets_.begin());
        }
    }
};
}  // namespace monitoring
}  // namespace drogon
