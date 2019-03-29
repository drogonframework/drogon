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
    if(config.isNull())
        LOG_DEBUG << "Configuration not defined";
    _interval = config.get("heartbeat_interval", 1).asInt();
    _workThread = std::thread([this]() {
        while(!_stop)
        {
            LOG_DEBUG << "TestPlugin heartbeat!";
            sleep(_interval);
        }
    });
}

void TestPlugin::shutdown() 
{
    /// Shutdown the plugin
    _stop = true;
    _workThread.join();
}
