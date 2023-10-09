/**
 *
 *  @file Redirector.cc
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

#include <drogon/drogon.h>
#include <drogon/plugins/Redirector.h>

using namespace drogon;
using namespace drogon::plugin;

void Redirector::initAndStart(const Json::Value &config)
{
    auto weakPtr = std::weak_ptr<Redirector>(shared_from_this());
    drogon::app().registerSyncAdvice(
        [weakPtr](const HttpRequestPtr &req) -> HttpResponsePtr {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
            {
                return HttpResponsePtr{};
            }
            std::string protocol, host;
            bool pathChanged{false};
            for (const auto &handler : thisPtr->preRedirectorHandlers_)
            {
                if (!handler(req, protocol, host, pathChanged))
                {
                    return HttpResponse::newNotFoundResponse(req);
                }
            }
            for (const auto &handler : thisPtr->pathRedirectorHandlers_)
            {
                pathChanged |= handler(req);
            }
            for (const auto &handler : thisPtr->postRedirectorHandlers_)
            {
                if (!handler(req, host, pathChanged))
                {
                    return HttpResponse::newNotFoundResponse();
                }
            }
            if (!protocol.empty() || !host.empty() || pathChanged)
            {
                std::string url;
                const std::string &path = req->path();
                const auto &query = req->query();
                const bool hasQuery = !query.empty();
                if (protocol.empty())
                {
                    if (!host.empty())
                    {
                        const bool isOnSecureConnection =
                            req->isOnSecureConnection();
                        url.reserve(
                            (isOnSecureConnection ? 8 /*https*/ : 7 /*http*/) +
                            host.size() + path.size() +
                            (hasQuery ? (1 + query.size()) : 0));
                        url = isOnSecureConnection ? "https://" : "http://";
                        url.append(host);
                    }
                    else
                        url.reserve(path.size() +
                                    (hasQuery ? (1 + query.size()) : 0));
                }
                else
                {
                    const std::string &newHost =
                        host.empty() ? req->getHeader("host") : host;
                    url.reserve(protocol.size() + newHost.size() + path.size() +
                                (hasQuery ? (1 + query.size()) : 0));
                    url = std::move(protocol);
                    url.append(newHost);
                }
                url.append(path);
                if (hasQuery)
                {
                    url.append("?").append(query);
                }
                return HttpResponse::newRedirectionResponse(url);
            }
            for (const auto &handler : thisPtr->pathForwarderHandlers_)
            {
                handler(req);
            }
            return HttpResponsePtr{};
        });
}

void Redirector::shutdown()
{
    LOG_TRACE << "Redirector plugin is shutdown!";
}
