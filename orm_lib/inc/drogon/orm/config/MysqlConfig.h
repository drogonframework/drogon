#pragma once
#include <string>

namespace drogon::orm
{
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
}  // namespace drogon::orm
