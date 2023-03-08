#include "JsonConfigAdapter.h"
#include <fstream>

using namespace drogon;
Json::Value JsonConfigAdapter::getJson(const std::string &configFile) const
    noexcept(false)
{
    Json::Value root;
    Json::Reader reader;
    std::ifstream in(configFile, std::ios::binary);
    if (!in.is_open())
    {
        throw std::runtime_error("Cannot open config file!");
    }
    in >> root;
    return root;
}
std::vector<std::string> JsonConfigAdapter::getExtensions() const
{
    return {"json"};
}
