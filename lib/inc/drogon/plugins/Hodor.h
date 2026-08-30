/**
 *  @file Hodor.h
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once
#include <drogon/RateLimiter.h>
#include <drogon/plugins/Plugin.h>
#include <drogon/plugins/RealIpResolver.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/CacheMap.h>
#include <regex>
#include <optional>

namespace drogon
{
namespace plugin
{
/**
 * @brief The Hodor plugin implements a global rate limiter that limits the
 * number of requests in a particular time unit.
 * The json configuration is as follows:
 *
 * @code
  {
     "name": "drogon::plugin::Hodor",
     "dependencies": [],
     "config": {
        // The algorithm used to limit the number of requests.
        // The default value is "token_bucket". other values are "fixed_window"
or "sliding_window".
        "algorithm": "token_bucket",
        // a regular expression (for matching the path of a request) list for
URLs that have to be limited. if the list is empty, all URLs are limited.
        "urls": ["^/api/.*", ...],
        // The time unit in seconds. the default value is 60.
        "time_unit": 60,
        // The maximum number of requests in a time unit. the default value 0
means no limit.
        "capacity": 0,
        // The maximum number of requests in a time unit for a single IP. the
default value 0 means no limit.
        "ip_capacity": 0,
        // The maximum number of requests in a time unit for a single user.
a function must be provided to the plugin to get the user id from the request.
the default value 0 means no limit.
        "user_capacity": 0,
        // Use the RealIpResolver plugin to get the real IP address of the
request. if this option is true, the RealIpResolver plugin should be added to
the dependencies list. the default value is false.
        "use_real_ip_resolver": false,
        // Multiple threads mode: the default value is true. if this option is
true, some mutexes are used for thread-safe.
        "multi_threads": true,
        // The message body of the response when the request is rejected.
        "rejection_message": "Too many requests",
        // In seconds, the minimum expiration time of the limiters for different
IPs or users. the default value is 600.
        "limiter_expire_time": 600,
        "sub_limits": [
            {
                "urls": ["^/api/1/.*", ...],
                "capacity": 0,
                "ip_capacity": 0,
                "user_capacity": 0
            },...
        ],
        // Trusted proxy ip or cidr
        "trust_ips": ["127.0.0.1", "172.16.0.0/12"],
     }
  }
  @endcode
 *
 * Enable the plugin by adding the configuration to the list of plugins in the
 * configuration file.
 * */
class DROGON_EXPORT Hodor : public drogon::Plugin<Hodor>
{
  public:
    Hodor()
    {
    }

    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

    /**
     * @brief the method is used to set a function to get the user id from the
     * request. users should call this method after calling the app().run()
     * method. etc. use the beginning advice of AOP.
     * */
    void setUserIdGetter(
        std::function<std::optional<std::string>(const HttpRequestPtr &)> func)
    {
        userIdGetter_ = std::move(func);
    }

    /**
     * @brief the method is used to set a function to create the response when
     * the rate limit is exceeded. users should call this method after calling
     * the app().run() method. etc. use the beginning advice of AOP.
     * */
    void setRejectResponseFactory(
        std::function<HttpResponsePtr(const HttpRequestPtr &)> func)
    {
        rejectResponseFactory_ = std::move(func);
    }

  private:
    struct LimitStrategy
    {
        std::regex urlsRegex;
        size_t capacity{0};
        size_t ipCapacity{0};
        size_t userCapacity{0};
        bool regexFlag{false};
        RateLimiterPtr globalLimiterPtr;
        std::unique_ptr<CacheMap<std::string, RateLimiterPtr>> ipLimiterMapPtr;
        std::unique_ptr<CacheMap<std::string, RateLimiterPtr>>
            userLimiterMapPtr;
    };

    LimitStrategy makeLimitStrategy(const Json::Value &config);
    std::vector<LimitStrategy> limitStrategies_;
    RateLimiterType algorithm_{RateLimiterType::kTokenBucket};
    std::chrono::duration<double> timeUnit_{1.0};
    bool multiThreads_{true};
    bool useRealIpResolver_{false};
    size_t limiterExpireTime_{600};
    std::function<std::optional<std::string>(const drogon::HttpRequestPtr &)>
        userIdGetter_;
    std::function<HttpResponsePtr(const drogon::HttpRequestPtr &)>
        rejectResponseFactory_;

    RealIpResolver::CIDRs trustCIDRs_;

    void onHttpRequest(const drogon::HttpRequestPtr &,
                       AdviceCallback &&,
                       AdviceChainCallback &&);
    bool checkLimit(const drogon::HttpRequestPtr &req,
                    const LimitStrategy &strategy,
                    const trantor::InetAddress &ip,
                    const std::optional<std::string> &userId);
    HttpResponsePtr rejectResponse_;
};
}  // namespace plugin
}  // namespace drogon
