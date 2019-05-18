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
typedef std::unique_ptr<PluginBase> PluginBasePtr;

class PluginsManager : trantor::NonCopyable
{
  public:
    void initializeAllPlugins(
        const Json::Value &configs,
        const std::function<void(PluginBase *)> &forEachCallback);

    PluginBase *getPlugin(const std::string &pluginName);
    ~PluginsManager();

  private:
    std::map<std::string, PluginBasePtr> _pluginsMap;
    std::vector<PluginBase *> _initializedPlugins;
};

}  // namespace drogon