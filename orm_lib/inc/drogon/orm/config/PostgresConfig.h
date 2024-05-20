#pragma once
#include <string>
#include <unordered_map>

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
}  // namespace drogon::orm
