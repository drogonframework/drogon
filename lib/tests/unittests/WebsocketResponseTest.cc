#include <drogon/drogon_test.h>

#include "../../lib/src/HttpRequestImpl.h"
#include "../../lib/src/HttpResponseImpl.h"
#include "../../lib/src/HttpControllerBinder.h"

using namespace drogon;

DROGON_TEST(WebsocketReponseTest)
{
    WebsocketControllerBinder binder;

    auto reqPtr = std::make_shared<HttpRequestImpl>(nullptr);

    // Value from rfc6455-1.3
    reqPtr->addHeader("sec-websocket-key", "dGhlIHNhbXBsZSBub25jZQ==");

    binder.handleRequest(reqPtr, [&](const HttpResponsePtr &resp) {
        CHECK(resp->statusCode() == k101SwitchingProtocols);
        CHECK(resp->headers().size() == 3);
        CHECK(resp->getHeader("Upgrade") == "websocket");
        CHECK(resp->getHeader("Connection") == "Upgrade");

        // Value from rfc6455-1.3
        CHECK(resp->getHeader("Sec-WebSocket-Accept") ==
              "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");

        auto implPtr = std::dynamic_pointer_cast<HttpResponseImpl>(resp);

        implPtr->makeHeaderString();
        auto buffer = implPtr->renderToBuffer();
        auto str = std::string{buffer->peek(), buffer->readableBytes()};

        CHECK(str.find("upgrade: websocket") != std::string::npos);
        CHECK(str.find("connection: Upgrade") != std::string::npos);
        CHECK(str.find("sec-websocket-accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") !=
              std::string::npos);
        CHECK(str.find("content-length:") == std::string::npos);
    });
}
