/**
 *  @file Plugin.h
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

#include <drogon/DrObject.h>
#include <json/json.h>
#include <memory>
#include <trantor/utils/Logger.h>
#include <trantor/utils/NonCopyable.h>

namespace drogon
{
enum class PluginStatus
{
    None,
    Initializing,
    Initialized
};

/**
 * @brief The abstract base class for plugins.
 *
 */
class DROGON_EXPORT PluginBase : public virtual DrObjectBase,
                                 public trantor::NonCopyable
{
  public:
    /// This method must be called by drogon.
    void initialize()
    {
        if (status_ == PluginStatus::None)
        {
            status_ = PluginStatus::Initializing;
            for (auto dependency : dependencies_)
            {
                dependency->initialize();
            }
            initAndStart(config_);
            status_ = PluginStatus::Initialized;
            if (initializedCallback_)
                initializedCallback_(this);
        }
        else if (status_ == PluginStatus::Initialized)
        {
            // Do nothing;
        }
        else
        {
            LOG_FATAL << "There are a circular dependency within plugins.";
            abort();
        }
    }

    /// This method must be called by drogon to initialize and start the plugin.
    /// It must be implemented by the user.
    virtual void initAndStart(const Json::Value &config) = 0;

    /// This method must be called by drogon to shutdown the plugin.
    /// It must be implemented by the user.
    virtual void shutdown() = 0;

    virtual ~PluginBase()
    {
    }

  protected:
    PluginBase()
    {
    }

  private:
    PluginStatus status_{PluginStatus::None};
    friend class PluginsManager;
    void setConfig(const Json::Value &config)
    {
        config_ = config;
    }
    void addDependency(PluginBase *dp)
    {
        dependencies_.push_back(dp);
    }
    void setInitializedCallback(const std::function<void(PluginBase *)> &cb)
    {
        initializedCallback_ = cb;
    }
    Json::Value config_;
    std::vector<PluginBase *> dependencies_;
    std::function<void(PluginBase *)> initializedCallback_;
};

template <typename T>
struct IsPlugin
{
    using TYPE =
        typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    static int test(void *)
    {
        return 0;
    }
    static char test(PluginBase *)
    {
        return 0;
    }
    static constexpr bool value =
        (sizeof(test((TYPE *)nullptr)) == sizeof(char));
};

/**
 * @brief The reflection base class for plugins.
 *
 * @tparam T The type of the implementation plugin classes.
 */
template <typename T>
class Plugin : public PluginBase, public DrObject<T>
{
  public:
    virtual ~Plugin()
    {
    }

  protected:
    Plugin()
    {
    }
};

}  // namespace drogon
