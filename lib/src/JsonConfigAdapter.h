#pragma once
#include "ConfigAdapter.h"
namespace drogon
{
class JsonConfigAdapter : public ConfigAdapter
{
  public:
    JsonConfigAdapter() = default;
    ~JsonConfigAdapter() override = default;
    Json::Value getJson(const std::string &content) const
        noexcept(false) override;
    std::vector<std::string> getExtensions() const override;
};
}  // namespace drogon