#include <drogon/plugins/Hodor.h>
#include <drogon/plugins/RealIpResolver.h>

using namespace drogon::plugin;
Hodor::LimitStrategy Hodor::makeLimitStrategy(const Json::Value &config)
{
    LimitStrategy strategy;
    strategy.capacity = config.get("capacity", 0).asUInt();
    if (config.isMember("urls") && config["urls"].isArray())
    {
        std::string regexString;
        for (auto &str : config["urls"])
        {
            assert(str.isString());
            regexString.append("(").append(str.asString()).append(")|");
        }
        if (!regexString.empty())
        {
            regexString.resize(regexString.length() - 1);
            strategy.urlsRegex = std::regex(regexString);
            strategy.regexFlag = true;
        }
    }

    if (strategy.capacity > 0)
    {
        if (multiThreads_)
        {
            strategy.globalLimiterPtr = std::make_shared<SafeRateLimiter>(
                RateLimiter::newRateLimiter(algorithm_,
                                            strategy.capacity,
                                            timeUnit_));
        }
        else
        {
            strategy.globalLimiterPtr =
                RateLimiter::newRateLimiter(algorithm_,
                                            strategy.capacity,
                                            timeUnit_);
        }
    }
    strategy.ipCapacity = config.get("ip_capacity", 0).asUInt();
    if (strategy.ipCapacity > 0)
    {
        strategy.ipLimiterMapPtr =
            std::make_unique<CacheMap<std::string, RateLimiterPtr>>(
                drogon::app().getLoop(),
                float(timeUnit_.count() / 60 < 1 ? 1 : timeUnit_.count() / 60),
                2,
                100);
    }

    strategy.userCapacity = config.get("user_capacity", 0).asUInt();
    if (strategy.userCapacity > 0)
    {
        strategy.userLimiterMapPtr =
            std::make_unique<CacheMap<std::string, RateLimiterPtr>>(
                drogon::app().getLoop(),
                float(timeUnit_.count() / 60 < 1 ? 1 : timeUnit_.count() / 60),
                2,
                100);
    }
    return strategy;
}
void Hodor::initAndStart(const Json::Value &config)
{
    algorithm_ = stringToRateLimiterType(
        config.get("algorithm", "token_bucket").asString());
    timeUnit_ = std::chrono::seconds(config.get("time_unit", 60).asUInt());

    multiThreads_ = config.get("multi_threads", true).asBool();

    useRealIpResolver_ = config.get("use_real_ip_resolver", false).asBool();
    rejectResponse_ = HttpResponse::newHttpResponse();
    rejectResponse_->setStatusCode(k429TooManyRequests);
    rejectResponse_->setBody(
        config.get("rejection_message", "Too many requests").asString());
    rejectResponse_->setCloseConnection(true);
    limiterExpireTime_ =
        (std::min)(static_cast<size_t>(
                       config.get("limiter_expire_time", 600).asUInt()),
                   static_cast<size_t>(timeUnit_.count() * 3));
    limitStrategies_.emplace_back(makeLimitStrategy(config));
    if (config.isMember("sub_limits") && config["sub_limits"].isArray())
    {
        for (auto &subLimit : config["sub_limits"])
        {
            assert(subLimit.isObject());
            if (!subLimit["urls"].isArray() || subLimit["urls"].size() == 0)
            {
                LOG_ERROR
                    << "The urls of sub_limits must be an array and not empty!";
                continue;
            }
            if (subLimit["capacity"].asUInt() == 0 &&
                subLimit["ip_capacity"].asUInt() == 0 &&
                subLimit["user_capacity"].asUInt() == 0)
            {
                LOG_ERROR << "At least one capacity of sub_limits must be "
                             "greater than 0!";
                continue;
            }
            limitStrategies_.emplace_back(makeLimitStrategy(subLimit));
        }
    }
    app().registerPreHandlingAdvice([this](const HttpRequestPtr &req,
                                           AdviceCallback &&acb,
                                           AdviceChainCallback &&accb) {
        onHttpRequest(req, std::move(acb), std::move(accb));
    });
}

void Hodor::shutdown()
{
    LOG_TRACE << "Hodor plugin is shutdown!";
}
bool Hodor::checkLimit(const HttpRequestPtr &req,
                       const LimitStrategy &strategy,
                       const std::string &ip,
                       const optional<std::string> &userId)
{
    if (strategy.regexFlag)
    {
        if (!std::regex_match(req->path(), strategy.urlsRegex))
        {
            return true;
        }
    }
    if (strategy.globalLimiterPtr)
    {
        if (!strategy.globalLimiterPtr->isAllowed())
        {
            return false;
        }
    }
    if (strategy.ipCapacity > 0)
    {
        RateLimiterPtr limiterPtr;
        strategy.ipLimiterMapPtr->modify(
            ip,
            [this, &limiterPtr, &strategy](RateLimiterPtr &ptr) {
                if (!ptr)
                {
                    if (multiThreads_)
                    {
                        ptr = std::make_shared<SafeRateLimiter>(
                            RateLimiter::newRateLimiter(algorithm_,
                                                        strategy.ipCapacity,
                                                        timeUnit_));
                    }
                    else
                    {
                        ptr = RateLimiter::newRateLimiter(algorithm_,
                                                          strategy.ipCapacity,
                                                          timeUnit_);
                    }
                }
                limiterPtr = ptr;
            },
            limiterExpireTime_);
        if (!limiterPtr->isAllowed())
        {
            return false;
        }
    }
    if (strategy.userCapacity > 0)
    {
        if (!userId.has_value())
        {
            return true;
        }
        RateLimiterPtr limiterPtr;
        strategy.userLimiterMapPtr->modify(
            *userId,
            [this, &strategy, &limiterPtr](RateLimiterPtr &ptr) {
                if (!ptr)
                {
                    if (multiThreads_)
                    {
                        ptr = std::make_shared<SafeRateLimiter>(
                            RateLimiter::newRateLimiter(algorithm_,
                                                        strategy.userCapacity,
                                                        timeUnit_));
                    }
                    else
                    {
                        ptr = RateLimiter::newRateLimiter(algorithm_,
                                                          strategy.userCapacity,
                                                          timeUnit_);
                    }
                }
                limiterPtr = ptr;
            },
            limiterExpireTime_);
        if (!limiterPtr->isAllowed())
        {
            return false;
        }
    }
    return true;
}
void Hodor::onHttpRequest(const drogon::HttpRequestPtr &req,
                          drogon::AdviceCallback &&adviceCallback,
                          drogon::AdviceChainCallback &&chainCallback)
{
    std::string ip;
    if (useRealIpResolver_)
    {
        ip = drogon::plugin::RealIpResolver::GetRealAddr(req).toIp();
    }
    else
    {
        ip = req->peerAddr().toIp();
    }
    optional<std::string> userId;
    if (userIdGetter_)
    {
        userId = userIdGetter_(req);
    }
    for (auto &strategy : limitStrategies_)
    {
        if (!checkLimit(req, strategy, ip, userId))
        {
            if (rejectResponseFactory_)
            {
                adviceCallback(rejectResponseFactory_(req));
            }
            else
            {
                adviceCallback(rejectResponse_);
            }
            return;
        }
    }
    chainCallback();
}
