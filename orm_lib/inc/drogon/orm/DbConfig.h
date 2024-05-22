/**
 *
 *  @file DbConfig.h
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

#include <string>
#include <unordered_map>
#include <variant>
#include <drogon/config.h>

namespace drogon::orm
{
#if USE_POSTGRESQL
struct PostgresConfig
{
    std::string host;
    unsigned short port;
    std::string databaseName;
    std::string username;
    std::string password;
    size_t connectionNumber;
    std::string name;
    bool isFast;
    std::string characterSet;
    double timeout;
    bool autoBatch;
    std::unordered_map<std::string, std::string> connectOptions;
};
#endif

#if USE_MYSQL
struct MysqlConfig
{
    std::string host;
    unsigned short port;
    std::string databaseName;
    std::string username;
    std::string password;
    size_t connectionNumber;
    std::string name;
    bool isFast;
    std::string characterSet;
    double timeout;
};
#endif

#if USE_SQLITE3
struct Sqlite3Config
{
    size_t connectionNumber;
    std::string filename;
    std::string name;
    double timeout;
};
#endif

using DbConfig = std::variant<
#if USE_POSTGRESQL
    PostgresConfig,
#endif
#if USE_MYSQL
    MysqlConfig,
#endif
#if USE_SQLITE3
    Sqlite3Config,
#endif
    int  // dummy type
    >;

}  // namespace drogon::orm
