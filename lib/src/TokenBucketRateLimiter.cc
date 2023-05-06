#include "TokenBucketRateLimiter.h"

using namespace drogon;

TokenBucketRateLimiter::TokenBucketRateLimiter(
    size_t capacity,
    std::chrono::duration<double> timeUnit)
    : capacity_(capacity),
      lastTime_(std::chrono::steady_clock::now()),
      timeUnit_(timeUnit),
      tokens_((double)capacity_)
{
}

// implementation of the token bucket algorithm
bool TokenBucketRateLimiter::isAllowed()
{
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(
        now - lastTime_);
    tokens_ += capacity_ * (duration / timeUnit_);
    if (tokens_ > capacity_)
        tokens_ = (double)capacity_;
    lastTime_ = now;
    if (tokens_ > 1.0)
    {
        tokens_ -= 1.0;
        return true;
    }
    return false;
}
