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

DROGON_TEST(HttpOptionsHeadersResponse)
{
    auto req = HttpRequest::newHttpRequest();
    auto resp = HttpResponse::newOptionsResponse(req);
    CHECK(!resp);

    req->setMethod(HttpMethod::Options);
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Vary") == "Origin");
    CHECK(resp->getHeader("Allow") == "OPTIONS");
    CHECK(resp->getHeader("Access-Control-Allow-Origin") == "");
    CHECK(resp->getHeader("Access-Control-Allow-Methods") == "");

    req->attributes()->insert("drogon.corsMethods", std::string("GET, POST, OPTIONS"));
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Vary") == "Origin");
    CHECK(resp->getHeader("Allow") == "GET, POST, OPTIONS");
    CHECK(resp->getHeader("Access-Control-Allow-Origin") == "");
    CHECK(resp->getHeader("Access-Control-Allow-Methods") == "");

    req->addHeader("Origin", "http://somepage");
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Vary") == "Origin");
    CHECK(resp->getHeader("Allow") == "GET, POST, OPTIONS");
    CHECK(resp->getHeader("Access-Control-Allow-Origin") == "");
    CHECK(resp->getHeader("Access-Control-Allow-Methods") == "");
}

DROGON_TEST(HttpCorsHeadersResponse)
{
    auto req = HttpRequest::newHttpRequest();
    req->addHeader("Origin", "");
    req->addHeader("Access-Control-Request-Method", "OPTIONS");
    auto resp = HttpResponse::newOptionsResponse(req);
    CHECK(!resp);

    req->setMethod(HttpMethod::Options);
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k400BadRequest);

    req->addHeader("Origin", "null");
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k403Forbidden);
    resp = HttpResponse::newOptionsResponse(req, {}, true);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Allow-Origin") == "null");

    req->addHeader("Origin", "http://somepage");
    req->addHeader("Access-Control-Request-Method", "");
    resp = HttpResponse::newOptionsResponse(req, {}, true);
    CHECK(resp->getStatusCode() == HttpStatusCode::k400BadRequest);

    req->addHeader("Access-Control-Request-Method", "OPTIONS");
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Vary") == "Origin");
    CHECK(resp->getHeader("Allow") == "");
    CHECK(resp->getHeader("Access-Control-Allow-Origin") == "http://somepage");
    CHECK(resp->getHeader("Access-Control-Allow-Methods") == "OPTIONS");

    resp = HttpResponse::newOptionsResponse(req, [](std::string_view origin) {
        return origin == "http://somepage";
    });
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    resp = HttpResponse::newOptionsResponse(req, [](std::string_view origin) {
        return origin != "http://somepage";
    });
    CHECK(resp->getStatusCode() == HttpStatusCode::k403Forbidden);

    req->addHeader("Access-Control-Request-Method", "PUT");
    req->attributes()->insert("drogon.corsMethods", std::string("GET,POST,OPTIONS"));
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k405MethodNotAllowed);
    CHECK(resp->getHeader("Allow") == "GET,POST,OPTIONS");
    CHECK(resp->getHeader("Access-Control-Allow-Methods") ==
          "GET,POST,OPTIONS");

    req->addHeader("Access-Control-Request-Method", "GET");
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Allow") == "");
    CHECK(resp->getHeader("Access-Control-Allow-Origin") == "http://somepage");
    CHECK(resp->getHeader("Access-Control-Allow-Methods") ==
          "GET,POST,OPTIONS");
    CHECK(resp->getHeader("Access-Control-Allow-Credentials") == "");
    CHECK(resp->getHeader("Access-Control-Allow-Private-Network") == "");
    CHECK(resp->getHeader("Access-Control-Max-Age") == "");

    req->addHeader("Access-Control-Request-Headers", "X-Foo, X-Bar");
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Allow-Headers") == "X-Bar,X-Foo");

    resp = HttpResponse::newOptionsResponse(req, {"X-Foo"});
    CHECK(resp->getStatusCode() == HttpStatusCode::k403Forbidden);
    CHECK(resp->getHeader("Access-Control-Allow-Headers") == "X-Foo");

    resp = HttpResponse::newOptionsResponse(req, {"X-Foo", "X-Bar"});
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Allow-Headers") == "X-Bar,X-Foo");

    resp = HttpResponse::newOptionsResponse(req, nullptr, false, true);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Allow-Credentials") == "true");

    req->addHeader("Access-Control-Request-Private-Network", "true");
    resp = HttpResponse::newOptionsResponse(req, nullptr, false, false, false);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Allow-Private-Network") == "");
    resp = HttpResponse::newOptionsResponse(req, nullptr, false, false, true);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Allow-Private-Network") == "true");

    resp =
        HttpResponse::newOptionsResponse(req, nullptr, false, false, false, 600);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Max-Age") == "600");
}
