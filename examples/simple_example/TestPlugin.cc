/**
 *
 *  TestPlugin.cc
 *
 */

#include "TestPlugin.h"
#include <thread>
#include <chrono>
using namespace std::chrono_literals;

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
            std::this_thread::sleep_for(std::chrono::seconds(interval_));
        }
    });
}

void TestPlugin::shutdown()
{
    /// Shutdown the plugin
    stop_ = true;
    workThread_.join();
}
