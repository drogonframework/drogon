/**
 *
 *  Metric.h
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

#include <drogon/utils/monitoring/Sample.h>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

namespace drogon
{
namespace monitoring
{
/**
 * This class is used to collect samples for a metric.
 * */
class Metric : public std::enable_shared_from_this<Metric>
{
  public:
    /**
     * Construct a metric with a name and a help string.
     * */

    Metric(const std::string &name,
           const std::vector<std::string> &labelNames,
           const std::vector<std::string> &labelValues) noexcept(false)
        : name_(name)
    {
        if (labelNames.size() != labelValues.size())
        {
            throw std::runtime_error(
                "The number of label names is not equal to the number of label "
                "values!");
        }
        labels_.resize(labelNames.size());
        for (size_t i = 0; i < labelNames.size(); i++)
        {
            labels_[i].first = labelNames[i];
            labels_[i].second = labelValues[i];
        }
    };

    const std::string &name() const
    {
        return name_;
    }

    const std::vector<std::pair<std::string, std::string>> &labels() const
    {
        return labels_;
    }

    virtual ~Metric() = default;
    virtual std::vector<Sample> collect() const = 0;

  protected:
    const std::string name_;
    std::vector<std::pair<std::string, std::string>> labels_;
};

using MetricPtr = std::shared_ptr<Metric>;

}  // namespace monitoring
}  // namespace drogon
