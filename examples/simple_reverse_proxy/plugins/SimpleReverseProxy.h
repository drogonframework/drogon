/**
 *
 *  @file SimpleReverseProxy.h
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
    void initAndStart(const Json::Value &config) override;

    /// This method must be called by drogon to shutdown the plugin.
    /// It must be implemented by the user.
    void shutdown() override;

  private:
    // Create 'connectionFactor_' HTTP clients for every backend in every IO
    // event loop.
    drogon::IOThreadStorage<std::vector<drogon::HttpClientPtr>> clients_;
    drogon::IOThreadStorage<size_t> clientIndex_{0};
    std::vector<std::string> backendAddrs_;
    bool sameClientToSameBackend_{false};
    size_t pipeliningDepth_{0};
    void preRouting(const drogon::HttpRequestPtr &,
                    drogon::AdviceCallback &&,
                    drogon::AdviceChainCallback &&);
    size_t connectionFactor_{1};
};
}  // namespace my_plugin
