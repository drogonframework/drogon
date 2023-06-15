/**
 *  @file PromExporter.h
 *  @author An Tao
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
#include <drogon/plugins/Plugin.h>
#include <drogon/utils/monitoring/Registry.h>
#include <memory>
#include <mutex>

namespace drogon
{
namespace plugin
{
/**
 * @brief The PromExporter plugin implements a prometheus exporter.
 * The json configuration is as follows:
 * @code
   {
      "name": "drogon::plugin::PromExporter",
      "dependencies": [],
      "config": {
         // The path of the metrics. the default value is "/metrics".
         "path": "/metrics"
         }
    }
    @endcode
 * */
class DROGON_EXPORT PromExporter
    : public drogon::Plugin<PromExporter>,
      public std::enable_shared_from_this<PromExporter>,
      public drogon::monitoring::Registry
{
  public:
    PromExporter()
    {
    }
    void initAndStart(const Json::Value &config) override;
    void shutdown() override
    {
    }
    ~PromExporter() override
    {
    }
    void registerCollector(
        const std::shared_ptr<drogon::monitoring::CollectorBase> &collector)
        override;

  private:
    std::mutex mutex_;
    std::vector<std::shared_ptr<drogon::monitoring::CollectorBase>> collectors_;
    std::string path_{"/metrics"};
    std::string exportMetrics();
};
}  // namespace plugin
}  // namespace drogon
