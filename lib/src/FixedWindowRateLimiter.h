#pragma once

#include <drogon/RateLimiter.h>
#include <chrono>
namespace drogon
{
class FixedWindowRateLimiter : public RateLimiter
{
  public:
    FixedWindowRateLimiter(size_t capacity,
                           std::chrono::duration<double> timeUnit);
    virtual bool isAllowed() override;

  private:
    size_t capacity_;
    size_t currentRequests_{0};
    std::chrono::steady_clock::time_point lastTime_;
    std::chrono::duration<double> timeUnit_;
};
}  // namespace drogon
