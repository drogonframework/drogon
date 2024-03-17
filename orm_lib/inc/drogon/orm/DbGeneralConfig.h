/**
 *
 *  @file DbGeneralConfig.h
 *  @author Nitromelon
 *
 *  Copyright 2024, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace drogon::orm
{

struct DbGeneralConfig
{
    std::string dbType;
    std::string host;
    unsigned short port;
    std::string databaseName;
    std::string username;
    std::string password;
    size_t connectionNumber;
    std::string filename;
    std::string name;
    bool isFast;
    std::string characterSet;
    double timeout;
    bool autoBatch;
    std::unordered_map<std::string, std::string> connectOptions;
};

}  // namespace drogon::orm
