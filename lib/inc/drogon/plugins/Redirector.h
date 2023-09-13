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
#include <trantor/net/InetAddress.h>
#include <drogon/HttpRequest.h>
#include <vector>

namespace drogon
{
namespace plugin
{
using RedirectorHandler =
    std::function<bool(const drogon::HttpRequestPtr &,
                       std::string &,    //"http://" or "https://"
                       std::string &,    // host
                       std::string &)>;  // path

/**
 * @brief This plugin is used to redirect requests to proper URLs. It is a
 * helper plugin for other plugins, e.g. SlashRemover.
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

    void registerHandler(RedirectorHandler &&handler)
    {
        handlers_.emplace_back(std::move(handler));
    }

    void registerHandler(const RedirectorHandler &handler)
    {
        handlers_.emplace_back(handler);
    }

  private:
    std::vector<RedirectorHandler> handlers_;
};

}  // namespace plugin
}  // namespace drogon