// Minimal test shim for MSVC shared builds to provide small, portable
// implementations used by `HttpRequestBodyCacheTest` without pulling in
// platform-specific sources like CacheFile.cc.

#if defined(_MSC_VER) && defined(BUILD_SHARED_LIBS)

#include "../../lib/src/HttpAppFrameworkImpl.h"
#include "../../lib/src/HttpRequestImpl.h"

namespace drogon
{

// Provide a lightweight setUploadPath implementation so tests can set
// upload path without linking to the full HttpAppFrameworkImpl object
// implementation in the DLL (which may not export this symbol on MSVC).
HttpAppFramework &HttpAppFrameworkImpl::setUploadPath(
    const std::string &uploadPath)
{
    uploadPath_ = uploadPath;
    return *this;
}

// Simplified, in-memory implementations for body handling used by the test.
void HttpRequestImpl::reserveBodySize(size_t /*length*/)
{
    // For tests, allow storing body in-memory to avoid file-backed cache.
}

void HttpRequestImpl::appendToBody(const char *data, size_t length)
{
    realContentLength_ += length;
    // Store everything in memory for the shim
    content_.append(data, length);
}

void HttpRequestImpl::appendToBuffer(trantor::MsgBuffer *output) const
{
    // Minimal serialization: METHOD SP PATH [?query] SP HTTP/1.1\r\n
    switch (method_)
    {
        case Get:
            output->append("GET ");
            break;
        case Post:
            output->append("POST ");
            break;
        case Head:
            output->append("HEAD ");
            break;
        case Put:
            output->append("PUT ");
            break;
        case Delete:
            output->append("DELETE ");
            break;
        default:
            output->append("GET ");
            break;
    }

    if (!path_.empty())
    {
        output->append(path_);
    }
    else
    {
        output->append("/");
    }

    if (!query_.empty())
    {
        output->append("?");
        output->append(query_);
    }

    output->append(" HTTP/1.1\r\n");

    // content-length header
    if (!content_.empty())
    {
        char buf[64];
        auto len = snprintf(buf, sizeof(buf), "content-length: %zu\r\n",
                            content_.length());
        output->append(buf, len);
    }
    else if (method_ == Post || method_ == Put)
    {
        output->append("content-length: 0\r\n");
    }

    // Append a blank line then the body
    output->append("\r\n");
    if (!content_.empty())
        output->append(content_);
}

}  // namespace drogon

#endif
