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

namespace drogon::orm
{
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

struct Sqlite3Config
{
    size_t connectionNumber;
    std::string filename;
    std::string name;
    double timeout;
};

// Duckdb configuration structure [dq 2025-11-19]
struct DuckdbConfig
{
    size_t connectionNumber;
    std::string filename;
    std::string name;
    double timeout;
    std::unordered_map<std::string, std::string> configOptions;  // DuckDB configuration options [dq 2025-11-19]
};

using DbConfig = std::variant<PostgresConfig, MysqlConfig, Sqlite3Config, DuckdbConfig>;

}  // namespace drogon::orm
