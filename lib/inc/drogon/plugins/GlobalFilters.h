#pragma once

#include <drogon/plugins/Plugin.h>
#include <regex>
#include <vector>
#include <memory>
#include <drogon/HttpFilter.h>

namespace drogon
{
namespace plugin
{
/**
 * @brief This plugin is used to add global filters to all HTTP requests.
 * The json configuration is as follows:
 *
 * @code
   {
        "name": "drogon::plugin::GlobalFilters",
        "dependencies": [],
        "config": {
            // filters: the list of global filter names.
            "filters": [
                "FilterName1", "FilterName2",...
            ],
            // exempt: exempt must be a string or string array, regular
 expressions for
            // URLs that don't have to be filtered.
            "exempt": [
                "^/static/.*\\.css", "^/images/.*",...
            ]
        }
   }
   @endcode
 *
 */
class DROGON_EXPORT GlobalFilters
    : public drogon::Plugin<GlobalFilters>,
      public std::enable_shared_from_this<GlobalFilters>
{
  public:
    GlobalFilters()
    {
    }

    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

  private:
    std::vector<std::shared_ptr<drogon::HttpFilterBase>> filters_;
    std::regex exemptPegex_;
    bool regexFlag_{false};
};
}  // namespace plugin
}  // namespace drogon
