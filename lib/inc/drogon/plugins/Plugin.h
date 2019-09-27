/**
 *  Plugin.h
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

#include <drogon/DrObject.h>
#include <json/json.h>
#include <memory>
#include <trantor/utils/Logger.h>
#include <trantor/utils/NonCopyable.h>

namespace drogon
{
enum class PluginState
{
    None,
    Initializing,
    Initialized
};

/**
 * @brief The abstract base class for plugins.
 *
 */
class PluginBase : public virtual DrObjectBase, public trantor::NonCopyable
{
  public:
    /// This method is usually called by drogon.
    /// It always returns PlugiinState::Initialized if the user calls it.
    PluginState stat() const
    {
        return _stat;
    }

    /// This method must be called by drogon.
    void initialize()
    {
        if (_stat == PluginState::None)
        {
            _stat = PluginState::Initializing;
            for (auto dependency : _dependencies)
            {
                dependency->initialize();
            }
            initAndStart(_config);
            _stat = PluginState::Initialized;
            if (_initializedCallback)
                _initializedCallback(this);
        }
        else if (_stat == PluginState::Initialized)
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
    PluginState _stat = PluginState::None;
    friend class PluginsManager;
    void setConfig(const Json::Value &config)
    {
        _config = config;
    }
    void addDependency(PluginBase *dp)
    {
        _dependencies.push_back(dp);
    }
    void setInitializedCallback(const std::function<void(PluginBase *)> &cb)
    {
        _initializedCallback = cb;
    }
    Json::Value _config;
    std::vector<PluginBase *> _dependencies;
    std::function<void(PluginBase *)> _initializedCallback;
};

template <typename T>
struct IsPlugin
{
    typedef
        typename std::remove_cv<typename std::remove_reference<T>::type>::type
            TYPE;

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
