/**
 *
 *  ConfigLoader.h
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
    const Json::Value &jsonValue() const { return _configJsonRoot; }
    void load();

  private:
    std::string _configFile;
    Json::Value _configJsonRoot;
};
} // namespace drogon
