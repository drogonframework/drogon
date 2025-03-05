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
            for (auto &handler : thisPtr->pathRewriteHandlers_)
            {
                pathChanged |= handler(req);
            }
            for (auto &handler : thisPtr->handlers_)
            {
                if (!handler(req, protocol, host, pathChanged))
                {
                    return HttpResponse::newNotFoundResponse(req);
                }
            }
            if (!protocol.empty() || !host.empty() || pathChanged)
            {
                std::string url;
                if (protocol.empty())
                {
                    if (!host.empty())
                    {
                        url = req->isOnSecureConnection() ? "https://"
                                                          : "http://";
                        url.append(host);
                    }
                }
                else
                {
                    url = std::move(protocol);
                    if (!host.empty())
                    {
                        url.append(host);
                    }
                    else
                    {
                        url.append(req->getHeader("host"));
                    }
                }
                url.append(req->path());
                auto &query = req->query();
                if (!query.empty())
                {
                    url.append("?").append(query);
                }
                return HttpResponse::newRedirectionResponse(url);
            }
            for (auto &handler : thisPtr->forwardHandlers_)
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
