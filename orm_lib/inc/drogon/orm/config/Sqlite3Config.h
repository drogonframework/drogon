#pragma once
#include <string>

namespace drogon::orm
{
struct Sqlite3Config
{
    size_t connectionNumber;
    std::string filename;
    std::string name;
    double timeout;
};
}  // namespace drogon::orm
