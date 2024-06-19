/**
 *
 *  Collector.h
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
#include <trantor/utils/Date.h>
#include <drogon/utils/monitoring/Sample.h>
#include <drogon/utils/monitoring/Metric.h>
#include <drogon/utils/monitoring/Registry.h>
#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#include <map>
#include <algorithm>
#include <memory>

namespace drogon
{
namespace monitoring
{
struct SamplesGroup
{
    std::shared_ptr<Metric> metric;
    std::vector<Sample> samples;
};

class CollectorBase : public std::enable_shared_from_this<CollectorBase>
{
  public:
    virtual ~CollectorBase() = default;
    virtual std::vector<SamplesGroup> collect() const = 0;
    virtual const std::string &name() const = 0;
    virtual const std::string &help() const = 0;
    virtual const std::string_view type() const = 0;
};

/**
 * @brief The Collector class template is used to collect samples from a group
 * of metric.
 */
template <typename T>
class Collector : public CollectorBase
{
  public:
    Collector(const std::string &name,
              const std::string &help,
              const std::vector<std::string> &labelNames)
        : name_(name), help_(help), labelsNames_(labelNames)
    {
    }

    template <typename... Arguments>
    const std::shared_ptr<T> &metric(
        const std::vector<std::string> &labelValues,
        Arguments... args) noexcept(false)
    {
        if (labelValues.size() != labelsNames_.size())
        {
            throw std::runtime_error(
                "The number of label values is not equal to the number of "
                "label names!");
        }
        std::lock_guard<std::mutex> guard(mutex_);
        auto iter = metrics_.find(labelValues);
        if (iter != metrics_.end())
        {
            return iter->second;
        }
        auto metric =
            std::make_shared<T>(name_, labelsNames_, labelValues, args...);
        metrics_[labelValues] = metric;
        return metrics_[labelValues];
    }

    std::vector<SamplesGroup> collect() const override
    {
        std::lock_guard<std::mutex> guard(mutex_);
        std::vector<SamplesGroup> samples;
        for (auto &pair : metrics_)
        {
            SamplesGroup samplesGroup;
            auto &metric = pair.second;
            samplesGroup.metric = metric;
            auto metricSamples = metric->collect();
            samplesGroup.samples = std::move(metricSamples);
            samples.emplace_back(std::move(samplesGroup));
        }
        return samples;
    }

    const std::string &name() const override
    {
        return name_;
    }

    const std::string &help() const override
    {
        return help_;
    }

    const std::string_view type() const override
    {
        return T::type();
    }

    void registerTo(Registry &registry)
    {
        registry.registerCollector(shared_from_this());
    }

    const std::vector<std::string> &labelsNames() const
    {
        return labelsNames_;
    }

  private:
    const std::string name_;
    const std::string help_;
    const std::vector<std::string> labelsNames_;
    std::map<std::vector<std::string>, std::shared_ptr<T>> metrics_;
    mutable std::mutex mutex_;
};
}  // namespace monitoring
}  // namespace drogon
