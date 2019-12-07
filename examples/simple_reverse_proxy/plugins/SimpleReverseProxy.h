/**
 *
 *  plugin_SimpleReverseProxy.h
 *
 */

#pragma once

#include <drogon/plugins/Plugin.h>
#include <drogon/drogon.h>
#include <vector>
#include <memory>

namespace my_plugin
{
class SimpleReverseProxy : public drogon::Plugin<SimpleReverseProxy>
{
  public:
    SimpleReverseProxy()
    {
    }
    /// This method must be called by drogon to initialize and start the plugin.
    /// It must be implemented by the user.
    virtual void initAndStart(const Json::Value &config) override;

    /// This method must be called by drogon to shutdown the plugin.
    /// It must be implemented by the user.
    virtual void shutdown() override;

  private:
    // Create a HTTP client for every backend in every IO event loop.
    drogon::IOThreadStorage<std::vector<drogon::HttpClientPtr>> clients_;
    drogon::IOThreadStorage<size_t> clientIndex_{0};
    std::vector<std::string> backendAddrs_;
    bool sameClientToSameBackend_{false};
    size_t cacheTimeout_{0};
    std::mutex mapMutex_;
    size_t pipeliningDepth_{0};
    void preRouting(const drogon::HttpRequestPtr &,
                    drogon::AdviceCallback &&,
                    drogon::AdviceChainCallback &&);
    std::unique_ptr<drogon::CacheMap<std::string, size_t>> clientMap_;
};
}  // namespace my_plugin
