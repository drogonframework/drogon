/**
 *  @file Redirector.h
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

#include <drogon/plugins/Plugin.h>
#include <drogon/HttpRequest.h>
#include <vector>

namespace drogon
{
namespace plugin
{
/**
 * @brief The RedirectorHandler is a function object that can be registered to
 * the Redirector plugin. It is used to redirect requests to proper URLs. Users
 * can modify the protocol, host and path of the request. If a false value is
 * returned, the request will be considered as invalid and a 404 response will
 * be sent to the client.
 */
using RedirectorHandler =
    std::function<bool(const drogon::HttpRequestPtr &,
                       std::string &,  //"http://" or "https://"
                       std::string &,  // host
                       bool &)>;       // path changed or not
/**
 * @brief The PathRewriteHandler is a function object that can be registered to
 * the Redirector plugin. It is used to rewrite the path of the request. The
 * Redirector plugin will call all registered PathRewriteHandlers in the order
 * of registration. If one or more handlers return true, the request will be
 * redirected to the new path.
 */
using PathRewriteHandler = std::function<bool(const drogon::HttpRequestPtr &)>;

/**
 * @brief The ForwardHandler is a function object that can be registered to the
 * Redirector plugin. It is used to forward the request to next processing steps
 * in the framework. The Redirector plugin will call all registered
 * ForwardHandlers in the order of registration. Users can use this handler to
 * change the request path or any other part of the request.
 */
using ForwardHandler = std::function<void(const drogon::HttpRequestPtr &)>;

/**
 * @brief This plugin is used to redirect requests to proper URLs. It is a
 * helper plugin for other plugins, e.g. SlashRemover.
 * Users can register a handler to this plugin to redirect requests. All
 * handlers will be called in the order of registration.
 * The json configuration is as follows:
 *
 * @code
   {
      "name": "drogon::plugin::Redirector",
      "dependencies": [],
      "config": {
      }
   }
   @endcode
 *
 */
class DROGON_EXPORT Redirector : public drogon::Plugin<Redirector>,
                                 public std::enable_shared_from_this<Redirector>
{
  public:
    Redirector()
    {
    }

    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

    void registerRedirectHandler(RedirectorHandler &&handler)
    {
        handlers_.emplace_back(std::move(handler));
    }

    void registerRedirectHandler(const RedirectorHandler &handler)
    {
        handlers_.emplace_back(handler);
    }

    void registerPathRewriteHandler(PathRewriteHandler &&handler)
    {
        pathRewriteHandlers_.emplace_back(std::move(handler));
    }

    void registerPathRewriteHandler(const PathRewriteHandler &handler)
    {
        pathRewriteHandlers_.emplace_back(handler);
    }

    void registerForwardHandler(ForwardHandler &&handler)
    {
        forwardHandlers_.emplace_back(std::move(handler));
    }

    void registerForwardHandler(const ForwardHandler &handler)
    {
        forwardHandlers_.emplace_back(handler);
    }

  private:
    std::vector<RedirectorHandler> handlers_;
    std::vector<PathRewriteHandler> pathRewriteHandlers_;
    std::vector<ForwardHandler> forwardHandlers_;
};

}  // namespace plugin
}  // namespace drogon
