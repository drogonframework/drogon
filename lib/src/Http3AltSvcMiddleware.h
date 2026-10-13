/**
 *
 *  @file Http3AltSvcMiddleware.h
 *  @author S Bala Vignesh
 *
 *  Copyright 2026, S Bala Vignesh. All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#ifdef DROGON_HAS_HTTP3

#include <drogon/HttpMiddleware.h>
#include <string>

namespace drogon
{

/**
 * @brief Middleware that injects the Alt-Svc header into HTTP/1.1 and HTTP/2
 * responses to advertise HTTP/3 support to clients.
 *
 * When a browser receives an "Alt-Svc: h3=\":443\"; ma=86400" header,
 * it knows it can upgrade to HTTP/3 for subsequent requests.
 *
 * Usage in config.json:
 * @code
 * {
 *   "middlewares": [
 *     {
 *       "name": "drogon::Http3AltSvcMiddleware",
 *       "config": {
 *         "port": 443
 *       }
 *     }
 *   ]
 * }
 * @endcode
 */
class Http3AltSvcMiddleware
    : public HttpMiddleware<Http3AltSvcMiddleware>
{
  public:
    Http3AltSvcMiddleware() = default;

    /**
     * @brief Set the QUIC port for the Alt-Svc header.
     * @param port UDP port where HTTP/3 is available
     */
    void setPort(uint16_t port)
    {
        altSvcValue_ = "h3=\":" + std::to_string(port) + "\"; ma=86400";
    }

    void invoke(const HttpRequestPtr &req,
                MiddlewareNextCallback &&nextCb,
                MiddlewareCallback &&mcb) override
    {
        auto altSvc = altSvcValue_;
        nextCb([altSvc = std::move(altSvc),
                mcb = std::move(mcb)](const HttpResponsePtr &resp) {
            // Only inject Alt-Svc on non-HTTP/3 responses
            if (resp && resp->getHeader("alt-svc").empty())
            {
                resp->addHeader("alt-svc", altSvc);
            }
            mcb(resp);
        });
    }

  private:
    std::string altSvcValue_{"h3=\":443\"; ma=86400"};
};

}  // namespace drogon

#endif  // DROGON_HAS_HTTP3
