#include "FixedWindowRateLimiter.h"

using namespace drogon;

FixedWindowRateLimiter::FixedWindowRateLimiter(
    size_t capacity,
    std::chrono::duration<double> timeUnit)
    : capacity_(capacity),
      lastTime_(std::chrono::steady_clock::now()),
      timeUnit_(timeUnit)
{
}

// implementation of the fixed window algorithm

bool FixedWindowRateLimiter::isAllowed()
{
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(
        now - lastTime_);
    if (duration >= timeUnit_)
    {
        currentRequests_ = 0;
        lastTime_ = now;
    }
    if (currentRequests_ < capacity_)
    {
        currentRequests_++;
        return true;
    }
    return false;
}
