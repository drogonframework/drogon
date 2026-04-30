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

    // removing header
    req->removeHeader("Abc");
    CHECK(req->getHeader("abc") == "");

    req->addHeader("Def", "def");
    CHECK(req->getHeader("Def") == "def");

    auto it = req->headers().find("def");
    const std::string *original_ptr = &it->second;

    // clearing header, but reusing memory
    req->clearHeader("Def");
    CHECK(req->getHeader("def") == "");
    CHECK(&req->getHeader("def") == original_ptr);
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

    req->attributes()->insert("drogon.corsMethods",
                              std::string("GET, POST, OPTIONS"));
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

    // empty origin -> error
    req->setMethod(HttpMethod::Options);
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k400BadRequest);

    // null origin -> check if allowed or not
    req->addHeader("Origin", "null");
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k403Forbidden);
    resp = HttpResponse::newOptionsResponse(req, {}, true);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Allow-Origin") == "null");

    // normal origin but no requested method -> error
    req->addHeader("Origin", "http://somepage");
    req->addHeader("Access-Control-Request-Method", "");
    resp = HttpResponse::newOptionsResponse(req, {}, true);
    CHECK(resp->getStatusCode() == HttpStatusCode::k400BadRequest);

    // valid CORS preflight request
    req->addHeader("Access-Control-Request-Method", "OPTIONS");
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Vary") == "Origin");
    CHECK(resp->getHeader("Allow") == "");
    CHECK(resp->getHeader("Access-Control-Allow-Origin") == "http://somepage");
    CHECK(resp->getHeader("Access-Control-Allow-Methods") == "OPTIONS");

    // origin validator
    resp = HttpResponse::newOptionsResponse(req, [](std::string_view origin) {
        return origin == "http://somepage";
    });
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    resp = HttpResponse::newOptionsResponse(req, [](std::string_view origin) {
        return origin != "http://somepage";
    });
    CHECK(resp->getStatusCode() == HttpStatusCode::k403Forbidden);

    // unallowed method
    req->addHeader("Access-Control-Request-Method", "PUT");
    req->attributes()->insert("drogon.corsMethods",
                              std::string("GET,POST,OPTIONS"));
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k405MethodNotAllowed);
    CHECK(resp->getHeader("Allow") == "GET,POST,OPTIONS");
    CHECK(resp->getHeader("Access-Control-Allow-Methods") ==
          "GET,POST,OPTIONS");

    // allowed method
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

    // no restriction on requested headers
    req->addHeader("Access-Control-Request-Headers", "X-Foo, X-Bar");
    resp = HttpResponse::newOptionsResponse(req);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Allow-Headers") == "X-Bar,X-Foo");

    // unallowed header
    resp = HttpResponse::newOptionsResponse(req, {"X-Foo"});
    CHECK(resp->getStatusCode() == HttpStatusCode::k403Forbidden);
    CHECK(resp->getHeader("Access-Control-Allow-Headers") == "X-Foo");

    // all requested headers allowed
    resp = HttpResponse::newOptionsResponse(req, {"X-Foo", "X-Bar"});
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Allow-Headers") == "X-Bar,X-Foo");

    // allow credentials
    resp = HttpResponse::newOptionsResponse(req, nullptr, false, true);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Allow-Credentials") == "true");

    // private network access
    req->addHeader("Access-Control-Request-Private-Network", "true");
    resp = HttpResponse::newOptionsResponse(req, nullptr, false, false, false);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Allow-Private-Network") == "");
    resp = HttpResponse::newOptionsResponse(req, nullptr, false, false, true);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Allow-Private-Network") == "true");

    // CORS max age
    resp = HttpResponse::newOptionsResponse(
        req, nullptr, false, false, false, 600);
    CHECK(resp->getStatusCode() == HttpStatusCode::k204NoContent);
    CHECK(resp->getHeader("Access-Control-Max-Age") == "600");
}

DROGON_TEST(AddHttpCorsHeaders)
{
    using namespace std::literals;

    // no Origin -> do nothing
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Get);
    auto resp = HttpResponse::newHttpResponse();
    resp->addCorsHeaders(req, {"X-Foo"}, true);
    CHECK(resp->headers().empty());

    // with Origin -> Allow-Origin + Vary (not overwritten) + Expose-Headers
    req->addHeader("Origin", "http://somepage");
    resp->addHeader("Vary", "X-SomeHeader");
    resp->addCorsHeaders(req, {"X-Foo"});
    CHECK(resp->getHeader("Vary") == "Origin,X-SomeHeader");
    CHECK(resp->getHeader("Access-Control-Allow-Origin") == "http://somepage");
    CHECK(resp->getHeader("Access-Control-Expose-Headers") == "X-Foo");

    // add a new exposed header
    resp->addCorsHeaders(req, {"X-Bar"});
    CHECK(resp->getHeader("Access-Control-Expose-Headers") == "X-Bar,X-Foo");
    // no duplicate Origin in Vary
    CHECK(resp->getHeader("Vary") == "Origin,X-SomeHeader");

    // check credentials (true/false/unchanged)
    resp->addCorsHeaders(req, {}, true);
    CHECK(resp->getHeader("Access-Control-Expose-Headers") == "X-Bar,X-Foo");
    CHECK(resp->getHeader("Access-Control-Allow-Credentials") == "true");
    resp->addCorsHeaders(req, {}, false);
    CHECK(resp->getHeader("Access-Control-Allow-Credentials") == "");
    resp->addCorsHeaders(req, {}, true);
    resp->addCorsHeaders(req);
    CHECK(resp->getHeader("Access-Control-Allow-Credentials") == "true");
}
