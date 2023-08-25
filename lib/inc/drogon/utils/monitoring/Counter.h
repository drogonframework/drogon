/**
 *
 *  Counter.h
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
#include <string_view>
#include <mutex>

namespace drogon
{
namespace monitoring
{
/**
 * This class is used to collect samples for a counter metric.
 * */
class Counter : public Metric
{
  public:
    Counter(const std::string &name,
            const std::vector<std::string> &labelNames,
            const std::vector<std::string> &labelValues) noexcept(false)
        : Metric(name, labelNames, labelValues)
    {
    }

    std::vector<Sample> collect() const override
    {
        Sample s;
        s.name = name_;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            s.value = value_;
        }
        return {s};
    }

    /**
     * Increment the counter by 1.
     * */
    void increment()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        value_++;
    }

    /**
     * Increment the counter by the given value.
     * */
    void increment(double value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ += value;
    }

    void reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ = 0;
    }

    static std::string_view type()
    {
        return "counter";
    }

  private:
    mutable std::mutex mutex_;
    double value_{0};
};
}  // namespace monitoring
}  // namespace drogon
