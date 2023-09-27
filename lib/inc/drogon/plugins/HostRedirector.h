/**
 *  @file HostRedirector.h
 *  @author Mis1eader
 *
 *  Copyright 2023, Mis1eader.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include "drogon/plugins/Plugin.h"
#include "drogon/utils/FunctionTraits.h"
#include <json/value.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace drogon::plugin
{
/**
 * @brief The HostRedirector plugin redirects requests with respect to rules.
 * The json configuration is as follows:
 *
 * @code
  {
     "name": "drogon::plugin::HostRedirector",
     "dependencies": ["drogon::plugin::Redirector"],
     "config": {
         "rules": {
             "www.example.com": [
                 "10.10.10.1",
                 "example.com",
                 "search.example.com",
                 "ww.example.com"
             ],
             "images.example.com": [
                 "image.example.com",
                 "www.example.com/images",
                 "www.example.com/image"
             ],
             "www.example.com/maps": [
                 "www.example.com/map",
                 "map.example.com",
                 "maps.example.com"
             ]
         }
     }
  }
  @endcode
 *
 * Enable the plugin by adding the configuration to the list of plugins in the
 * configuration file.
 * */
class DROGON_EXPORT HostRedirector
    : public drogon::Plugin<HostRedirector>,
      public std::enable_shared_from_this<HostRedirector>
{
  private:
    struct RedirectRule
    {
        std::string redirectToHost, redirectToPath;
        std::unordered_set<std::string> paths;
    };

  public:
    HostRedirector()
    {
    }

    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

  private:
    bool redirectingAdvice(const drogon::HttpRequestPtr &,
                           std::string &,
                           bool &);

    std::unordered_map<std::string, RedirectRule> rules_;
};
}  // namespace drogon::plugin
