#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <filesystem>
#include <trantor/net/EventLoop.h>
#include <trantor/utils/MsgBuffer.h>
#include "../../lib/src/HttpAppFrameworkImpl.h"
#include "../../lib/src/HttpRequestImpl.h"

using namespace drogon;
using namespace trantor;

namespace
{
struct BodyLimitGuard
{
    HttpAppFrameworkImpl &app;
    size_t oldLimit;

    ~BodyLimitGuard()
    {
        app.setClientMaxMemoryBodySize(oldLimit);
    }
};

struct UploadPathGuard
{
    HttpAppFrameworkImpl &app;
    std::string oldPath;

    ~UploadPathGuard()
    {
        app.setUploadPath(oldPath);
    }
};
}  // namespace

DROGON_TEST(CachedRequestBodyIsSerialized)
{
    auto &appFramework = HttpAppFrameworkImpl::instance();
    BodyLimitGuard guard{appFramework,
                         appFramework.getClientMaxMemoryBodySize()};
    auto tempRoot = std::filesystem::temp_directory_path() /
                    "drogon-request-body-cache-test";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot);
    for (char hi : std::string{"0123456789abcdef"})
    {
        for (char lo : std::string{"0123456789abcdef"})
        {
            std::filesystem::create_directories(tempRoot / "tmp" /
                                               std::string{hi, lo});
        }
    }
    UploadPathGuard uploadGuard{appFramework, appFramework.getUploadPath()};
    appFramework.setUploadPath(tempRoot.string());
    appFramework.setClientMaxMemoryBodySize(1);

    EventLoop loop;
    HttpRequestImpl req(&loop);
    req.setMethod(Post);
    req.setPath("/forward");
    req.setVersion(Version::kHttp11);

    const std::string body(32, 'x');
    req.reserveBodySize(body.size());
    req.appendToBody(body.data(), body.size());

    MsgBuffer buffer;
    req.appendToBuffer(&buffer);

    std::string serialized(buffer.peek(), buffer.readableBytes());
    CHECK(serialized.find("content-length: 32\r\n") != std::string::npos);

    auto separatorPos = serialized.find("\r\n\r\n");
    REQUIRE(separatorPos != std::string::npos);
    CHECK(serialized.substr(separatorPos + 4) == body);

    std::filesystem::remove_all(tempRoot);
}
