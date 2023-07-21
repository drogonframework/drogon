/**
 *
 *  PluginsManager.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "PluginsManager.h"
#include <trantor/utils/Logger.h>

using namespace drogon;

PluginsManager::~PluginsManager()
{
    // Shut down all plugins in reverse order of initializaiton.
    for (auto iter = initializedPlugins_.rbegin();
         iter != initializedPlugins_.rend();
         iter++)
    {
        (*iter)->shutdown();
    }
}

void PluginsManager::initializeAllPlugins(
    const Json::Value &configs,
    const std::function<void(PluginBase *)> &forEachCallback)
{
    assert(configs.isArray());
    std::vector<PluginBase *> plugins;
    for (auto &config : configs)
    {
        auto name = config.get("name", "").asString();
        if (name.empty())
            continue;
        createPlugin(name);
    }
    for (auto &config : configs)
    {
        auto name = config.get("name", "").asString();
        if (name.empty())
            continue;
        auto pluginPtr = getPlugin(name);
        if (!pluginPtr)
        {
            continue;
        }
        auto configuration = config["config"];
        auto dependencies = config["dependencies"];
        pluginPtr->setConfig(configuration);
        assert(dependencies.isArray() || dependencies.isNull());
        if (dependencies.isArray())
        {
            // Is not null and is an array
            for (auto &depName : dependencies)
            {
                auto *dp = getPlugin(depName.asString());
                if (dp)
                {
                    pluginPtr->addDependency(dp);
                }
                else
                {
                    LOG_FATAL << "Dependent plugin " << depName.asString()
                              << " is not loaded";
                    abort();
                }
            }
        }
        pluginPtr->setInitializedCallback([this](PluginBase *p) {
            LOG_TRACE << "Plugin " << p->className() << " initialized!";
            initializedPlugins_.push_back(p);
        });
        plugins.push_back(pluginPtr);
    }
    // Initialize them, Depth first
    for (auto plugin : plugins)
    {
        plugin->initialize();
        forEachCallback(plugin);
    }
}
void PluginsManager::createPlugin(const std::string &pluginName)
{
    auto pluginPtr = std::dynamic_pointer_cast<PluginBase>(
        DrClassMap::newSharedObject(pluginName));
    if (!pluginPtr)
    {
        LOG_ERROR << "Plugin " << pluginName << " undefined!";
        return;
    }
    pluginsMap_[pluginName] = pluginPtr;
}
PluginBase *PluginsManager::getPlugin(const std::string &pluginName)
{
    auto iter = pluginsMap_.find(pluginName);
    if (iter != pluginsMap_.end())
    {
        return iter->second.get();
    }
    return nullptr;
}

std::shared_ptr<PluginBase> PluginsManager::getSharedPlugin(
    const std::string &pluginName)
{
    auto iter = pluginsMap_.find(pluginName);
    if (iter != pluginsMap_.end())
    {
        return iter->second;
    }
    return nullptr;
}