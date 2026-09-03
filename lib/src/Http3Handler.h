/**
 *
 *  @file Http3Handler.h
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

#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include <nghttp3/nghttp3.h>
#include <string>
#include <vector>
#include <utility>

namespace drogon
{

/**
 * @brief Http3Handler provides utility functions for converting between
 * HTTP/3 (nghttp3) representations and Drogon's HttpRequest/HttpResponse.
 *
 * It acts as the "glue" between the QUIC/HTTP/3 protocol layer and
 * Drogon's existing request processing pipeline.
 */
namespace Http3Handler
{

/**
 * @brief Create a Drogon HttpRequestImpl from HTTP/3 pseudo-headers.
 * @param method The HTTP method (e.g., "GET", "POST")
 * @param path The request path
 * @param authority The authority (host) header
 * @param scheme The scheme ("https")
 * @return A new HttpRequestImpl ready for header population
 */
HttpRequestImplPtr createRequest(const std::string &method,
                                 const std::string &path,
                                 const std::string &authority,
                                 const std::string &scheme);

/**
 * @brief Serialize an HttpResponse into nghttp3 header name-value pairs
 * for transmission over HTTP/3.
 * @param resp The response to serialize
 * @return A vector of nghttp3_nv header pairs
 * @note The returned nv pairs reference memory in the returned strings.
 *       The caller must keep the strings alive until headers are sent.
 */
struct SerializedHeaders
{
    std::vector<nghttp3_nv> nva;
    // Storage for header strings to keep them alive
    std::string statusStr;
    std::vector<std::pair<std::string, std::string>> headerStorage;
};

SerializedHeaders serializeResponseHeaders(const HttpResponsePtr &resp);

/**
 * @brief Get the response body as a contiguous buffer.
 * @param resp The response
 * @return Pointer and length of the body data
 */
std::pair<const uint8_t *, size_t> getResponseBody(
    const HttpResponsePtr &resp);

/**
 * @brief Convert an HTTP method string to Drogon's HttpMethod enum.
 * @param method The method string (e.g., "GET")
 * @return The corresponding HttpMethod
 */
HttpMethod methodFromString(const std::string &method);

/**
 * @brief Get the Alt-Svc header value for advertising HTTP/3 support.
 * @param port The UDP port for HTTP/3
 * @return The Alt-Svc header value string (e.g., 'h3=":443"')
 */
std::string getAltSvcHeaderValue(uint16_t port);

}  // namespace Http3Handler

}  // namespace drogon

#endif  // DROGON_HAS_HTTP3
