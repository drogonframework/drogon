#include "JsonConfigAdapter.h"
#include <fstream>
#include <mutex>

using namespace drogon;

Json::Value JsonConfigAdapter::getJson(const std::string &content) const
    noexcept(false)
{
    static std::once_flag once;
    static Json::CharReaderBuilder builder;
    std::call_once(once, []() { builder["collectComments"] = false; });
    JSONCPP_STRING errs;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    if (!reader->parse(
            content.c_str(), content.c_str() + content.size(), &root, &errs))
    {
        throw std::runtime_error(errs);
    }
    return root;
}

std::vector<std::string> JsonConfigAdapter::getExtensions() const
{
    return {"json"};
}
