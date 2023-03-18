#pragma once
#include "ConfigAdapterManager.h"
#include "ConfigAdapter.h"
#include <map>
namespace drogon
{
class ConfigAdapterManager
{
  public:
    static ConfigAdapterManager &instance();
    Json::Value getJson(const std::string &content, std::string ext) const
        noexcept(false);

  private:
    ConfigAdapterManager();
    std::map<std::string, ConfigAdapterPtr> adapters_;
};
}  // namespace drogon