#pragma once

#include <string>

namespace drogon
{
namespace orm
{
class DbListenerMixin
{
  public:
    virtual ~DbListenerMixin() = default;

    virtual void onMessage(const std::string& channel,
                           const std::string& message) const noexcept = 0;
    virtual void listenAll() noexcept = 0;
    virtual void listenNext() noexcept = 0;
};

}  // namespace orm
}  // namespace drogon
