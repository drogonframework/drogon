
#include "AOPAdvice.h"

namespace drogon
{

void doAdvicesChain(const std::vector<std::function<void(const HttpRequestPtr &,
                                                         const AdviceCallback &,
                                                         const AdviceChainCallback &)>> &advices,
                    size_t index,
                    const HttpRequestImplPtr &req,
                    const std::shared_ptr<const std::function<void(const HttpResponsePtr &)>> &callbackPtr,
                    std::function<void()> &&missCallback)
{
    if (index < advices.size())
    {
        auto &advice = advices[index];
        advice(req,
               *callbackPtr,
               [index, req, callbackPtr, &advices, missCallback = std::move(missCallback)]() mutable {
                   doAdvicesChain(advices, index + 1, req, callbackPtr, std::move(missCallback));
               });
    }
    else
    {
        missCallback();
    }
}

void doAdvicesChain(const std::deque<std::function<void(const HttpRequestPtr &,
                                                        const AdviceCallback &,
                                                        const AdviceChainCallback &)>> &advices,
                    size_t index,
                    const HttpRequestImplPtr &req,
                    const std::shared_ptr<const std::function<void(const HttpResponsePtr &)>> &callbackPtr,
                    std::function<void()> &&missCallback)
{
    if (index < advices.size())
    {
        auto &advice = advices[index];
        advice(req,
               *callbackPtr,
               [index, req, callbackPtr, &advices, missCallback = std::move(missCallback)]() mutable {
                   doAdvicesChain(advices, index + 1, req, callbackPtr, std::move(missCallback));
               });
    }
    else
    {
        missCallback();
    }
}

} // namespace drogon