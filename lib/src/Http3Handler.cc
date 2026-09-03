/**
 *
 *  @file Http3Handler.cc
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

#ifdef DROGON_HAS_HTTP3

#include "Http3Handler.h"
#include "HttpUtils.h"
#include <trantor/utils/Logger.h>
#include <sstream>

namespace drogon
{
namespace Http3Handler
{

HttpRequestImplPtr createRequest(const std::string &method,
                                 const std::string &path,
                                 const std::string &authority,
                                 const std::string &scheme)
{
    auto req = std::make_shared<HttpRequestImpl>(nullptr);
    req->setMethod(methodFromString(method));
    req->setPath(path);
    req->addHeader("host", authority);
    req->addHeader(":scheme", scheme);
    req->setVersion(drogon::Version::kHttp3);
    return req;
}

SerializedHeaders serializeResponseHeaders(const HttpResponsePtr &resp)
{
    SerializedHeaders result;

    // Status pseudo-header
    result.statusStr = std::to_string(resp->statusCode());

    // Build nghttp3_nv for :status
    nghttp3_nv statusNv;
    statusNv.name = reinterpret_cast<uint8_t *>(
        const_cast<char *>(":status"));
    statusNv.namelen = 7;
    statusNv.value = reinterpret_cast<uint8_t *>(
        const_cast<char *>(result.statusStr.c_str()));
    statusNv.valuelen = result.statusStr.size();
    statusNv.flags = NGHTTP3_NV_FLAG_NONE;
    result.nva.push_back(statusNv);

    // Regular headers
    auto respImpl =
        std::dynamic_pointer_cast<HttpResponseImpl>(resp);
    if (respImpl)
    {
        for (auto &[name, value] : respImpl->headers())
        {
            result.headerStorage.emplace_back(name, value);
            auto &stored = result.headerStorage.back();

            nghttp3_nv nv;
            nv.name = reinterpret_cast<uint8_t *>(
                const_cast<char *>(stored.first.c_str()));
            nv.namelen = stored.first.size();
            nv.value = reinterpret_cast<uint8_t *>(
                const_cast<char *>(stored.second.c_str()));
            nv.valuelen = stored.second.size();
            nv.flags = NGHTTP3_NV_FLAG_NONE;
            result.nva.push_back(nv);
        }
    }

    return result;
}

std::pair<const uint8_t *, size_t> getResponseBody(
    const HttpResponsePtr &resp)
{
    auto body = resp->body();
    return {reinterpret_cast<const uint8_t *>(body.data()), body.size()};
}

HttpMethod methodFromString(const std::string &method)
{
    if (method == "GET")
        return HttpMethod::Get;
    if (method == "POST")
        return HttpMethod::Post;
    if (method == "PUT")
        return HttpMethod::Put;
    if (method == "DELETE")
        return HttpMethod::Delete;
    if (method == "PATCH")
        return HttpMethod::Patch;
    if (method == "HEAD")
        return HttpMethod::Head;
    if (method == "OPTIONS")
        return HttpMethod::Options;
    return HttpMethod::Invalid;
}

std::string getAltSvcHeaderValue(uint16_t port)
{
    std::ostringstream oss;
    oss << "h3=\":" << port << "\"; ma=86400";
    return oss.str();
}

}  // namespace Http3Handler
}  // namespace drogon

#endif  // DROGON_HAS_HTTP3
