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
#include <drogon/utils/optional.h>
#include <drogon/CacheMap.h>
#include <regex>

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
        "capacity": 1000,
        // The maximum number of requests in a time unit for a single IP. the
default value 0 means no limit.
        "ip_capacity": 100,
        // The maximum number of requests in a time unit for a single user.
a function must be provided to the plugin to get the user id from the request.
the default value 0 means no limit. "user_capacity": 100,
        // Use the RealIpResolver plugin to get the real IP address of the
request. if this option is true, the RealIpResolver plugin should be added to
the dependencies list. the default value is false.
        "use_real_ip_resolver": false,
        // Multiple threads mode: the default value is true. if this option is
true, some mutexes are used to make it thread-safe.
        "multi_threads": true,
        // The message body of the response when the request is rejected.
        "rejection_message": "Too many requests"
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
    void initAndStart(const Json::Value &config) override;
    void shutdown() override;
    /**
     * @brief the method is used to set a function to get the user id from the
     * request. users should call this method after calling the app().run()
     * method. etc. use the beginning advice of AOP.
     * */
    void setUserIdGetter(
        std::function<optional<std::string>(const HttpRequestPtr)> func)
    {
        userIdGetter_ = std::move(func);
    }

  private:
    RateLimiterPtr globalLimiterPtr_;
    std::regex regex_;
    bool regexFlag_{false};
    RateLimiterType algorithm_{RateLimiterType::kTokenBucket};
    std::chrono::duration<double> timeUnit_{1.0};
    size_t capacity_{0};
    size_t ipCapacity_{0};
    size_t userCapacity_{0};
    bool multiThreads_{true};
    bool useRealIpResolver_{false};
    std::function<optional<std::string>(const drogon::HttpRequestPtr &)>
        userIdGetter_;
    std::unique_ptr<CacheMap<std::string, RateLimiterPtr>> ipLimiterMapPtr_;
    std::unique_ptr<CacheMap<std::string, RateLimiterPtr>> userLimiterMapPtr_;
    void onHttpRequest(const HttpRequestPtr &,
                       AdviceCallback &&,
                       AdviceChainCallback &&);
    HttpResponsePtr rejectResponse_;
};
}  // namespace plugin
}  // namespace drogon
