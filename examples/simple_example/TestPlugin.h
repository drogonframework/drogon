/**
 *
 *  TestPlugin.h
 *
 */

#pragma once

#include <drogon/plugins/Plugin.h>
using namespace drogon;

class TestPlugin : public Plugin<TestPlugin>
{
  public:
    TestPlugin()
    {
    }
    /// This method must be called by drogon to initialize and start the plugin.
    /// It must be implemented by the user.
    virtual void initAndStart(const Json::Value &config) override;

    /// This method must be called by drogon to shutdown the plugin.
    /// It must be implemented by the user.
    virtual void shutdown() override;

  private:
    std::thread workThread_;
    bool stop_{false};
    int interval_{0};
};
