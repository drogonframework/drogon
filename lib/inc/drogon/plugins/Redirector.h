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
 * @brief The PreRedirectorHandler is a function object that can be registered
 * to the Redirector plugin. It is used to redirect requests to proper URLs.
 * Users can modify the protocol, host and path of the request. If a false value
 * is returned, the request will be considered as invalid and a 404 response
 * will be sent to the client.
 */
using PreRedirectorHandler =
    std::function<bool(const drogon::HttpRequestPtr &,
                       std::string &,  //"http://" or "https://"
                       std::string &,  // host
                       bool &)>;       // path changed or not
/**
 * @brief The PathRedirectorHandler is a function object that can be registered
 * to the Redirector plugin. It is used to rewrite the path of the request. The
 * Redirector plugin will call all registered PathRedirectorHandlers in the
 * order of registration. If one or more handlers return true, the request will
 * be redirected to the new path.
 */
using PathRedirectorHandler =
    std::function<bool(const drogon::HttpRequestPtr &)>;

/**
 * @brief The PostRedirectorHandler is a function object that can be registered
 * to the Redirector plugin. It is used to redirect requests to proper URLs.
 * Users can modify the host or path of the request. If a false value is
 * returned, the request will be considered as invalid and a 404 response will
 * be sent to the client.
 */
using PostRedirectorHandler =
    std::function<bool(const drogon::HttpRequestPtr &,
                       std::string &,  // host
                       bool &)>;       // path changed or not

/**
 * @brief The PathForwarderHandler is a function object that can be registered
 * to the Redirector plugin. It is used to forward the request to next
 * processing steps in the framework. The Redirector plugin will call all
 * registered PathForwarderHandlers in the order of registration. Users can use
 * this handler to change the request path or any other part of the request.
 */
using PathForwarderHandler =
    std::function<void(const drogon::HttpRequestPtr &)>;

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

    void registerPreRedirectorHandler(PreRedirectorHandler &&handler)
    {
        preRedirectorHandlers_.emplace_back(std::move(handler));
    }

    void registerPreRedirectorHandler(const PreRedirectorHandler &handler)
    {
        preRedirectorHandlers_.emplace_back(handler);
    }

    void registerPathRedirectorHandler(PathRedirectorHandler &&handler)
    {
        pathRedirectorHandlers_.emplace_back(std::move(handler));
    }

    void registerPathRedirectorHandler(const PathRedirectorHandler &handler)
    {
        pathRedirectorHandlers_.emplace_back(handler);
    }

    void registerPostRedirectorHandler(PostRedirectorHandler &&handler)
    {
        postRedirectorHandlers_.emplace_back(std::move(handler));
    }

    void registerPostRedirectorHandler(const PostRedirectorHandler &handler)
    {
        postRedirectorHandlers_.emplace_back(handler);
    }

    void registerPathForwarderHandler(PathForwarderHandler &&handler)
    {
        pathForwarderHandlers_.emplace_back(std::move(handler));
    }

    void registerPathForwarderHandler(const PathForwarderHandler &handler)
    {
        pathForwarderHandlers_.emplace_back(handler);
    }

  private:
    std::vector<PreRedirectorHandler> preRedirectorHandlers_;
    std::vector<PathRedirectorHandler> pathRedirectorHandlers_;
    std::vector<PostRedirectorHandler> postRedirectorHandlers_;
    std::vector<PathForwarderHandler> pathForwarderHandlers_;
};

}  // namespace plugin
}  // namespace drogon
