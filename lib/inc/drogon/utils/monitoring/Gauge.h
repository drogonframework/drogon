/**
 *
 *  Gauge.h
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
#include <atomic>

namespace drogon
{
namespace monitoring
{
/**
 * This class is used to collect samples for a gauge metric.
 * */
class Gauge : public Metric
{
  public:
    /**
     * Construct a gauge metric with a name and a help string.
     * */
    Gauge(const std::string &name,
          const std::vector<std::string> &labelNames,
          const std::vector<std::string> &labelValues) noexcept(false)
        : Metric(name, labelNames, labelValues)
    {
    }

    std::vector<Sample> collect() const override
    {
        Sample s;
        std::lock_guard<std::mutex> lock(mutex_);
        s.name = name_;
        s.value = value_;
        s.timestamp = timestamp_;
        return {s};
    }

    /**
     * Increment the counter by 1.
     * */
    void increment()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ += 1;
    }

    void decrement()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ -= 1;
    }

    void decrement(double value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ -= value;
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

    void set(double value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ = value;
    }

    static std::string_view type()
    {
        return "counter";
    }

    void setToCurrentTime()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        timestamp_ = trantor::Date::now();
    }

  private:
    mutable std::mutex mutex_;
    double value_{0};
    trantor::Date timestamp_{0};
};
}  // namespace monitoring
}  // namespace drogon
