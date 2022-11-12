#include <drogon/RateLimiter.h>
#include "FixedWindowRateLimiter.h"
#include "SlidingWindowRateLimiter.h"
#include "TokenBucketRateLimiter.h"

using namespace drogon;

RateLimiterPtr RateLimiter::newRateLimiter(
    RateLimiterType type,
    size_t capacity,
    std::chrono::duration<double> timeUnit)
{
    switch (type)
    {
        case RateLimiterType::kFixedWindow:
            return std::make_shared<FixedWindowRateLimiter>(capacity, timeUnit);
        case RateLimiterType::kSlidingWindow:
            return std::make_shared<SlidingWindowRateLimiter>(capacity,
                                                              timeUnit);
        case RateLimiterType::kTokenBucket:
            return std::make_shared<TokenBucketRateLimiter>(capacity, timeUnit);
    }
    return std::make_shared<TokenBucketRateLimiter>(capacity, timeUnit);
}
