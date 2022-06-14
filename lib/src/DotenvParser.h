/**
 *
 *  @file DotenvParser.h
 *  @author akaahmedkamal
 *
 *  Copyright 2020, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <json/json.h>
#include <fstream>

namespace drogon
{
class DotenvParser
{
  public:
    explicit DotenvParser();
    ~DotenvParser();
    const Json::Value &jsonValue() const
    {
        return configJsonValue_;
    }
    void parse(std::ifstream &);

  private:
    Json::Value configJsonValue_;
};
}  // namespace drogon
