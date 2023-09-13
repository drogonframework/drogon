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
            std::string location;
            for (auto &handler : thisPtr->handlers_)
            {
                handler(req, location);
            }
            if (!location.empty())
            {
                return HttpResponse::newRedirectionResponse(location);
            }
            return HttpResponsePtr{};
        });
}

void Redirector::shutdown()
{
    LOG_TRACE << "Redirector plugin is shutdown!";
}
