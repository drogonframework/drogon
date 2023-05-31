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
#include <drogon/utils/string_view.h>
#include <atomic>

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
            const std::string &labelValues) noexcept(false)
        : Metric(name, labelNames, labelValues)
    {
    }
    std::vector<Sample> collect() const override
    {
        Sample s;
        s.name = name_;
        s.value = value_.load();
        return {s};
    }
    /**
     * Increment the counter by 1.
     * */
    void increment()
    {
        value_ += 1;
    }
    /**
     * Increment the counter by the given value.
     * */
    void increment(double value)
    {
        value_ += value;
    }
    void reset()
    {
        value_ = 0;
    }
    static string_view type()
    {
        return "counter";
    }

  private:
    std::atomic<double> value_{0};
};
}  // namespace monitoring
}  // namespace drogon

