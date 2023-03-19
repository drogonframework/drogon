#include "JsonConfigAdapter.h"
#include <fstream>

using namespace drogon;
Json::Value JsonConfigAdapter::getJson(const std::string &content) const
    noexcept(false)
{
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(content, root))
    {
        throw std::runtime_error("Failed to parse JSON");
    }
    return root;
}
std::vector<std::string> JsonConfigAdapter::getExtensions() const
{
    return {"json"};
}
