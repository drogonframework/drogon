#pragma once

#include <drogon/RateLimiter.h>

namespace drogon
{
class TokenBucketRateLimiter : public RateLimiter
{
  public:
    TokenBucketRateLimiter(size_t capacity,
                           std::chrono::duration<double> timeUnit);
    bool isAllowed() override;

  private:
    size_t capacity_;
    std::chrono::steady_clock::time_point lastTime_;
    std::chrono::duration<double> timeUnit_;
    double tokens_{0};
};
}  // namespace drogon
