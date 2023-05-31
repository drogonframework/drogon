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
class DROGON_EXPORT PromExporter : public drogon::Plugin<PromExporter>
{
};
}  // namespace plugin
}  // namespace drogon