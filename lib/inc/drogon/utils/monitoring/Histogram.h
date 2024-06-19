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
#include <drogon/exports.h>
#include <drogon/utils/monitoring/Metric.h>
#include <trantor/net/EventLoopThread.h>
#include <string_view>
#include <atomic>
#include <mutex>

namespace drogon
{
namespace monitoring
{
/**
 * This class is used to collect samples for a counter metric.
 * */
class DROGON_EXPORT Histogram : public Metric
{
  public:
    struct TimeBucket
    {
        std::vector<uint64_t> buckets;
        uint64_t count{0};
        double sum{0};
    };

    Histogram(const std::string &name,
              const std::vector<std::string> &labelNames,
              const std::vector<std::string> &labelValues,
              const std::vector<double> &bucketBoundaries,
              const std::chrono::duration<double> &maxAge,
              uint64_t timeBucketsCount,
              trantor::EventLoop *loop = nullptr) noexcept(false)
        : Metric(name, labelNames, labelValues),
          maxAge_(maxAge),
          timeBucketCount_(timeBucketsCount),
          bucketBoundaries_(bucketBoundaries)
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
        }
        timeBuckets_.emplace_back();
        timeBuckets_.back().buckets.resize(bucketBoundaries.size() + 1);
        // check the bucket boundaries are sorted
        for (size_t i = 1; i < bucketBoundaries.size(); i++)
        {
            if (bucketBoundaries[i] <= bucketBoundaries[i - 1])
            {
                throw std::runtime_error(
                    "The bucket boundaries must be sorted");
            }
        }
    }

    void observe(double value);
    std::vector<Sample> collect() const override;

    ~Histogram() override
    {
        if (timerId_ != trantor::InvalidTimerId)
        {
            loopPtr_->invalidateTimer(timerId_);
        }
    }

    static std::string_view type()
    {
        return "histogram";
    }

  private:
    std::deque<TimeBucket> timeBuckets_;
    std::unique_ptr<trantor::EventLoopThread> loopThreadPtr_;
    trantor::EventLoop *loopPtr_{nullptr};
    mutable std::mutex mutex_;
    std::chrono::duration<double> maxAge_;
    trantor::TimerId timerId_{trantor::InvalidTimerId};
    size_t timeBucketCount_{0};
    const std::vector<double> bucketBoundaries_;

    void rotateTimeBuckets()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        TimeBucket bucket;
        bucket.buckets.resize(bucketBoundaries_.size() + 1);
        timeBuckets_.emplace_back(std::move(bucket));
        if (timeBuckets_.size() > timeBucketCount_)
        {
            auto expiredTimeBucket = timeBuckets_.front();
            timeBuckets_.erase(timeBuckets_.begin());
        }
    }
};
}  // namespace monitoring
}  // namespace drogon
