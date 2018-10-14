/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#pragma once

#include <trantor/utils/NonCopyable.h>
#include <json/json.h>
#include <string>

namespace drogon
{
class ConfigLoader : public trantor::NonCopyable
{
  public:
    explicit ConfigLoader(const std::string &configFile);
    ~ConfigLoader();
    void load();

  private:
    std::string _configFile;
    Json::Value _configJsonRoot;
};
} // namespace drogon