#include <drogon/drogon_test.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include "../../lib/src/HttpResponseImpl.h"

using namespace drogon;

DROGON_TEST(HttpHeaderRequest)
{
    auto req = HttpRequest::newHttpRequest();
    req->addHeader("Abc", "abc");
    CHECK(req->getHeader("Abc") == "abc");
    CHECK(req->getHeader("abc") == "abc");

    req->removeHeader("Abc");
    CHECK(req->getHeader("abc") == "");
}
DROGON_TEST(HttpHeaderResponse)
{
    auto resp = std::dynamic_pointer_cast<HttpResponseImpl>(
        HttpResponse::newHttpResponse());
    REQUIRE(resp != nullptr);
    resp->addHeader("Abc", "abc");
    CHECK(resp->getHeader("Abc") == "abc");
    CHECK(resp->getHeader("abc") == "abc");
    resp->makeHeaderString();

    auto buffer = resp->renderToBuffer();
    auto str = std::string{buffer->peek(), buffer->readableBytes()};
    CHECK(str.find("abc") != std::string::npos);

    resp->removeHeader("Abc");
    buffer = resp->renderToBuffer();
    str = std::string{buffer->peek(), buffer->readableBytes()};
    CHECK(str.find("abc") == std::string::npos);
    CHECK(resp->getHeader("abc") == "");
}

DROGON_TEST(ResponseSetCustomContentTypeString)
{
    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeString("text/html");
    CHECK(resp->getContentType() == CT_TEXT_HTML);

    resp = HttpResponse::newHttpResponse();
    resp->setContentTypeString("image/bmp");
    CHECK(resp->getContentType() == CT_IMAGE_BMP);

    resp = HttpResponse::newHttpResponse();
    resp->setContentTypeString("thisdoesnotexist/unknown");
    CHECK(resp->getContentType() == CT_CUSTOM);
}

DROGON_TEST(ResquestSetCustomContentTypeString)
{
    auto req = HttpRequest::newHttpRequest();
    req->setContentTypeString("text/html");
    CHECK(req->getContentType() == CT_TEXT_HTML);

    req = HttpRequest::newHttpRequest();
    req->setContentTypeString("image/bmp");
    CHECK(req->getContentType() == CT_IMAGE_BMP);

    req = HttpRequest::newHttpRequest();
    req->setContentTypeString("thisdoesnotexist/unknown");
    CHECK(req->getContentType() == CT_CUSTOM);
}
