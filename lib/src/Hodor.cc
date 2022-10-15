#include <drogon/plugins/Hodor.h>
#include <drogon/plugins/RealIpResolver.h>

using namespace drogon::plugin;
void Hodor::initAndStart(const Json::Value &config)
{
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
            regex_ = std::regex(regexString);
            regexFlag_ = true;
        }
    }
    algorithm_ = stringToReateLimiterType(
        config.get("algorithm", "token_bucket").asString());
    timeUnit_ = std::chrono::seconds(config.get("time_unit", 60).asUInt());
    capacity_ = config.get("capacity", 0).asUInt();
    multiThreads_ = config.get("multi_threads", true).asBool();
    if (capacity_ > 0)
    {
        if (multiThreads_)
        {
            globalLimiterPtr_ = std::make_shared<SafeRateLimiter>(
                RateLimiter::newRateLimiter(algorithm_, capacity_, timeUnit_));
        }
        else
        {
            globalLimiterPtr_ =
                RateLimiter::newRateLimiter(algorithm_, capacity_, timeUnit_);
        }
    }
    ipCapacity_ = config.get("ip_capacity", 0).asUInt();
    if (ipCapacity_ > 0)
    {
        ipLimiterMapPtr_ =
            std::make_unique<CacheMap<std::string, RateLimiterPtr>>(
                drogon::app().getLoop(),
                timeUnit_.count() / 60 < 1 ? 1 : timeUnit_.count() / 60,
                2,
                100);
    }

    userCapacity_ = config.get("user_capacity", 0).asUInt();
    if (userCapacity_ > 0)
    {
        userLimiterMapPtr_ =
            std::make_unique<CacheMap<std::string, RateLimiterPtr>>(
                drogon::app().getLoop(),
                timeUnit_.count() / 60 < 1 ? 1 : timeUnit_.count() / 60,
                2,
                100);
    }

    useRealIpResolver_ = config.get("use_real_ip_resolver", false).asBool();
    rejectResponse_ = HttpResponse::newHttpResponse();
    rejectResponse_->setStatusCode(k429TooManyRequests);
    rejectResponse_->setBody(
        config.get("rejection_message", "Too many requests").asString());
    rejectResponse_->setCloseConnection(true);
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

void Hodor::onHttpRequest(const drogon::HttpRequestPtr &req,
                          drogon::AdviceCallback &&adviceCallback,
                          drogon::AdviceChainCallback &&chainCallback)
{
    if (regexFlag_)
    {
        if (!std::regex_match(req->path(), regex_))
        {
            chainCallback();
            return;
        }
    }
    if (globalLimiterPtr_)
    {
        if (!globalLimiterPtr_->isAllowed())
        {
            adviceCallback(rejectResponse_);
            return;
        }
    }
    if (ipCapacity_ > 0)
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
        RateLimiterPtr limiterPtr;
        ipLimiterMapPtr_->modify(ip, [this, &limiterPtr](RateLimiterPtr &ptr) {
            if (!ptr)
            {
                if (multiThreads_)
                {
                    ptr = std::make_shared<SafeRateLimiter>(
                        RateLimiter::newRateLimiter(algorithm_,
                                                    ipCapacity_,
                                                    timeUnit_));
                }
                else
                {
                    ptr = RateLimiter::newRateLimiter(algorithm_,
                                                      ipCapacity_,
                                                      timeUnit_);
                }
            }
            limiterPtr = ptr;
        });
        if (!limiterPtr->isAllowed())
        {
            adviceCallback(rejectResponse_);
            return;
        }
    }
    if (userCapacity_ > 0)
    {
        if (!userIdGetter_)
        {
            LOG_ERROR << "The user id getter is not set!";
            chainCallback();
            return;
        }
        auto userId = userIdGetter_(req);
        if (userId)
        {
            RateLimiterPtr limiterPtr;
            userLimiterMapPtr_->modify(
                *userId, [this, &limiterPtr](RateLimiterPtr &ptr) {
                    if (!ptr)
                    {
                        if (multiThreads_)
                        {
                            ptr = std::make_shared<SafeRateLimiter>(
                                RateLimiter::newRateLimiter(algorithm_,
                                                            userCapacity_,
                                                            timeUnit_));
                        }
                        else
                        {
                            ptr = RateLimiter::newRateLimiter(algorithm_,
                                                              userCapacity_,
                                                              timeUnit_);
                        }
                    }
                    limiterPtr = ptr;
                });
            if (!limiterPtr->isAllowed())
            {
                adviceCallback(rejectResponse_);
                return;
            }
        }
    }
    chainCallback();
}