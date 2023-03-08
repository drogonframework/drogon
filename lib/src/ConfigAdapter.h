#pragma once
#include <json/json.h>
#include <vector>
#include <string>
#include <memory>

namespace drogon
{
class ConfigAdapter
{
  public:
    virtual ~ConfigAdapter() = default;
    virtual Json::Value getJson(const std::string &configFile) const
        noexcept(false) = 0;
    virtual std::vector<std::string> getExtensions() const = 0;
};
using ConfigAdapterPtr = std::shared_ptr<ConfigAdapter>;

}  // namespace drogon