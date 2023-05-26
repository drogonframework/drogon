/**
 *  @file SlashRemover.h
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

namespace drogon::plugin
{
/**
 * @brief The SlashRemover plugin redirects requests to proper paths if they
 * contain excessive slashes.
 * The json configuration is as follows:
 *
 * @code
  {
     "name": "drogon::plugin::SlashRemover",
     "dependencies": [],
     "config": {
        // If true, it removes all trailing slashes, e.g.
///home// -> ///home
        "remove_trailing_slashes": true,
        // If true, it removes all duplicate slashes, e.g.
///home// -> /home/
        "remove_duplicate_slashes": true,
        // If true, redirects the request, otherwise forwards
internally.
        "redirect": true
     }
  }
  @endcode
 *
 * Enable the plugin by adding the configuration to the list of plugins in the
 * configuration file.
 * */
class DROGON_EXPORT SlashRemover : public drogon::Plugin<SlashRemover>
{
  public:
    SlashRemover()
    {
    }
    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

  private:
    bool trailingSlashes_{true}, duplicateSlashes_{true}, redirect_{true};
};
}  // namespace drogon::plugin
