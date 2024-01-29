/**
 *
 *  PluginsManager.h
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

#pragma once
#include <drogon/plugins/Plugin.h>
#include <map>

namespace drogon
{
using PluginBasePtr = std::shared_ptr<PluginBase>;

class PluginsManager : trantor::NonCopyable
{
  public:
    void initializeAllPlugins(
        const Json::Value &configs,
        const std::function<void(PluginBase *)> &forEachCallback);

    PluginBase *getPlugin(const std::string &pluginName);

    std::shared_ptr<PluginBase> getSharedPlugin(const std::string &pluginName);

    ~PluginsManager();

  private:
    void createPlugin(const std::string &pluginName);
    std::map<std::string, PluginBasePtr> pluginsMap_;
    std::vector<PluginBase *> initializedPlugins_;
};

}  // namespace drogon
