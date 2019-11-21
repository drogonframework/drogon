/**
 *
 *  TestPlugin.cc
 *
 */

#include "TestPlugin.h"
#include <unistd.h>

using namespace drogon;

void TestPlugin::initAndStart(const Json::Value &config)
{
    /// Initialize and start the plugin
    if (config.isNull())
        LOG_DEBUG << "Configuration not defined";
    interval_ = config.get("heartbeat_interval", 1).asInt();
    workThread_ = std::thread([this]() {
        while (!stop_)
        {
            LOG_DEBUG << "TestPlugin heartbeat!";
            sleep(interval_);
        }
    });
}

void TestPlugin::shutdown()
{
    /// Shutdown the plugin
    stop_ = true;
    workThread_.join();
}
