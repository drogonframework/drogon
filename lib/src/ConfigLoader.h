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

#include <json/json.h>
#include <string>
#include <trantor/utils/NonCopyable.h>

namespace drogon
{
enum ConfigSyntax
{
    kJSON,
    kDotENV
};

class ConfigLoader : public trantor::NonCopyable
{
  public:
    explicit ConfigLoader(const std::string &configFile);
    explicit ConfigLoader(const std::string &configFile, ConfigSyntax syntax);
    explicit ConfigLoader(const Json::Value &data);
    explicit ConfigLoader(Json::Value &&data);
    ~ConfigLoader();
    const Json::Value &jsonValue() const
    {
        return configJsonRoot_;
    }
    void load();

  private:
    std::string configFile_;
    Json::Value configJsonRoot_;
};
}  // namespace drogon
