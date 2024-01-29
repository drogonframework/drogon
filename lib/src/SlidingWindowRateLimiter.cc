#include "SlidingWindowRateLimiter.h"
#include <assert.h>

using namespace drogon;

SlidingWindowRateLimiter::SlidingWindowRateLimiter(
    size_t capacity,
    std::chrono::duration<double> timeUnit)
    : capacity_(capacity),
      unitStartTime_(std::chrono::steady_clock::now()),
      lastTime_(unitStartTime_),
      timeUnit_(timeUnit)
{
}

// implementation of the sliding window algorithm
bool SlidingWindowRateLimiter::isAllowed()
{
    auto now = std::chrono::steady_clock::now();
    unitStartTime_ =
        unitStartTime_ +
        std::chrono::duration_cast<decltype(unitStartTime_)::duration>(
            std::chrono::duration<double>(
                static_cast<double>(
                    (uint64_t)(std::chrono::duration_cast<
                                   std::chrono::duration<double>>(
                                   now - unitStartTime_)
                                   .count() /
                               timeUnit_.count())) *
                timeUnit_.count()));

    if (unitStartTime_ > lastTime_)
    {
        auto duration =
            std::chrono::duration_cast<std::chrono::duration<double>>(
                unitStartTime_ - lastTime_);
        auto startTime = lastTime_;
        if (duration >= timeUnit_)
        {
            previousRequests_ = 0;
        }
        else
        {
            previousRequests_ = currentRequests_;
        }
        currentRequests_ = 0;
    }
    auto coef = std::chrono::duration_cast<std::chrono::duration<double>>(
                    now - unitStartTime_) /
                timeUnit_;
    assert(coef <= 1.0);
    auto count = previousRequests_ * (1.0 - coef) + currentRequests_;
    if (count < capacity_)
    {
        currentRequests_++;
        lastTime_ = now;
        return true;
    }
    return false;
}
