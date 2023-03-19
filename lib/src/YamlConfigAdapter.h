#pragma once
#include "ConfigAdapter.h"
namespace drogon
{
class YamlConfigAdapter : public ConfigAdapter
{
  public:
    YamlConfigAdapter() = default;
    ~YamlConfigAdapter() override = default;
    Json::Value getJson(const std::string &content) const
        noexcept(false) override;
    std::vector<std::string> getExtensions() const override;
};
}  // namespace drogon