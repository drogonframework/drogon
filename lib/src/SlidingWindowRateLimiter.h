#pragma once
#include <drogon/RateLimiter.h>
#include <chrono>

namespace drogon
{
class SlidingWindowRateLimiter : public RateLimiter
{
  public:
    SlidingWindowRateLimiter(size_t capacity,
                             std::chrono::duration<double> timeUnit);
    bool isAllowed() override;
    ~SlidingWindowRateLimiter() noexcept override = default;

  private:
    size_t capacity_;
    size_t currentRequests_{0};
    size_t previousRequests_{0};
    std::chrono::steady_clock::time_point unitStartTime_;
    std::chrono::steady_clock::time_point lastTime_;
    std::chrono::duration<double> timeUnit_;
};
}  // namespace drogon
