/**
 *  @file HttpTypes.h
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
#include <atomic>
#include <thread>
#include <iostream>
#include <drogon/utils/string_view.h>
#include <trantor/utils/LogStream.h>

namespace drogon
{
enum HttpStatusCode
{
    kUnknown = 0,
    k100Continue = 100,
    k101SwitchingProtocols = 101,
    k102Processing = 102,
    k103EarlyHints = 103,
    k200OK = 200,
    k201Created = 201,
    k202Accepted = 202,
    k203NonAuthoritativeInformation = 203,
    k204NoContent = 204,
    k205ResetContent = 205,
    k206PartialContent = 206,
    k207MultiStatus = 207,
    k208AlreadyReported = 208,
    k226IMUsed = 226,
    k300MultipleChoices = 300,
    k301MovedPermanently = 301,
    k302Found = 302,
    k303SeeOther = 303,
    k304NotModified = 304,
    k305UseProxy = 305,
    k306Unused = 306,
    k307TemporaryRedirect = 307,
    k308PermanentRedirect = 308,
    k400BadRequest = 400,
    k401Unauthorized = 401,
    k402PaymentRequired = 402,
    k403Forbidden = 403,
    k404NotFound = 404,
    k405MethodNotAllowed = 405,
    k406NotAcceptable = 406,
    k407ProxyAuthenticationRequired = 407,
    k408RequestTimeout = 408,
    k409Conflict = 409,
    k410Gone = 410,
    k411LengthRequired = 411,
    k412PreconditionFailed = 412,
    k413RequestEntityTooLarge = 413,
    k414RequestURITooLarge = 414,
    k415UnsupportedMediaType = 415,
    k416RequestedRangeNotSatisfiable = 416,
    k417ExpectationFailed = 417,
    k418ImATeapot = 418,
    k421MisdirectedRequest = 421,
    k422UnprocessableEntity = 422,
    k423Locked = 423,
    k424FailedDependency = 424,
    k425TooEarly = 425,
    k426UpgradeRequired = 426,
    k428PreconditionRequired = 428,
    k429TooManyRequests = 429,
    k431RequestHeaderFieldsTooLarge = 431,
    k451UnavailableForLegalReasons = 451,
    k500InternalServerError = 500,
    k501NotImplemented = 501,
    k502BadGateway = 502,
    k503ServiceUnavailable = 503,
    k504GatewayTimeout = 504,
    k505HTTPVersionNotSupported = 505,
    k506VariantAlsoNegotiates = 506,
    k507InsufficientStorage = 507,
    k508LoopDetected = 508,
    k510NotExtended = 510,
    k511NetworkAuthenticationRequired = 511
};

enum class Version
{
    kUnknown = 0,
    kHttp10,
    kHttp11
};

enum ContentType
{
    CT_NONE = 0,
    CT_APPLICATION_JSON,
    CT_TEXT_PLAIN,
    CT_TEXT_HTML,
    CT_APPLICATION_X_FORM,
    CT_APPLICATION_X_JAVASCRIPT,
    CT_TEXT_CSS,
    CT_TEXT_XML,
    CT_APPLICATION_XML,
    CT_TEXT_XSL,
    CT_APPLICATION_WASM,
    CT_APPLICATION_OCTET_STREAM,
    CT_APPLICATION_X_FONT_TRUETYPE,
    CT_APPLICATION_X_FONT_OPENTYPE,
    CT_APPLICATION_FONT_WOFF,
    CT_APPLICATION_FONT_WOFF2,
    CT_APPLICATION_VND_MS_FONTOBJ,
    CT_APPLICATION_PDF,
    CT_IMAGE_SVG_XML,
    CT_IMAGE_PNG,
    CT_IMAGE_WEBP,
    CT_IMAGE_AVIF,
    CT_IMAGE_JPG,
    CT_IMAGE_GIF,
    CT_IMAGE_XICON,
    CT_IMAGE_ICNS,
    CT_IMAGE_BMP,
    CT_MULTIPART_FORM_DATA,
    CT_CUSTOM
};

enum FileType
{
    FT_UNKNOWN = 0,
    FT_CUSTOM,
    FT_DOCUMENT,
    FT_ARCHIVE,
    FT_AUDIO,
    FT_MEDIA,
    FT_IMAGE
};

enum HttpMethod
{
    Get = 0,
    Post,
    Head,
    Put,
    Delete,
    Options,
    Patch,
    Invalid
};

enum class ReqResult
{
    Ok = 0,
    BadResponse,
    NetworkFailure,
    BadServerAddress,
    Timeout,
    HandshakeError,
    InvalidCertificate,
};

enum class WebSocketMessageType
{
    Text = 0,
    Binary,
    Ping,
    Pong,
    Close,
    Unknown
};

inline string_view to_string_view(drogon::ReqResult result)
{
    switch (result)
    {
        case ReqResult::Ok:
            return "OK";
        case ReqResult::BadResponse:
            return "Bad response from server";
        case ReqResult::NetworkFailure:
            return "Network failure";
        case ReqResult::BadServerAddress:
            return "Bad server address";
        case ReqResult::Timeout:
            return "Timeout";
        case ReqResult::HandshakeError:
            return "Handshake error";
        case ReqResult::InvalidCertificate:
            return "Invalid certificate";
        default:
            return "Unknown error";
    }
}

inline std::string to_string(drogon::ReqResult result)
{
    return to_string_view(result).data();
}

inline std::ostream &operator<<(std::ostream &out, drogon::ReqResult result)
{
    return out << to_string_view(result);
}

inline trantor::LogStream &operator<<(trantor::LogStream &out,
                                      drogon::ReqResult result)
{
    return out << to_string_view(result);
}
}  // namespace drogon
