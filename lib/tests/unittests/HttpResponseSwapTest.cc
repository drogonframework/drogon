#include <drogon/drogon_test.h>
#include <drogon/HttpResponse.h>
#include "../../lib/src/HttpResponseImpl.h"

using namespace drogon;

static std::string render(HttpResponseImpl &resp)
{
    auto buffer = resp.renderToBuffer();
    return std::string{buffer->peek(), buffer->readableBytes()};
}

DROGON_TEST(HttpResponseSwapExchangesEveryMember)
{
    HttpResponseImpl one;
    one.setStatusCode(k200OK);
    one.setContentTypeCode(CT_TEXT_PLAIN);
    one.setBody("one");
    one.setExpiredTime(0);
    one.setCloseConnection(true);

    HttpResponseImpl two;
    two.setStatusCode(k404NotFound);
    two.setContentTypeCode(CT_APPLICATION_JSON);
    two.setBody("two");
    two.setExpiredTime(-1);

    // Render first, so both objects carry whatever they memoize internally.
    render(one);
    render(two);

    one.swap(two);

    CHECK(one.statusCode() == k404NotFound);
    CHECK(two.statusCode() == k200OK);

    // contentType_ and contentTypeString_ are a pair: the enum drives
    // contentType(), the string drives the rendered header. Moving only one of
    // them leaves a response that contradicts itself.
    CHECK(one.contentType() == CT_APPLICATION_JSON);
    CHECK(two.contentType() == CT_TEXT_PLAIN);

    CHECK(one.expiredTime() == -1);
    CHECK(two.expiredTime() == 0);

    CHECK(one.ifCloseConnection() == false);
    CHECK(two.ifCloseConnection() == true);
    CHECK(one.closeConnectionSetByUser() == false);
    CHECK(two.closeConnectionSetByUser() == true);

    auto oneStr = render(one);
    auto twoStr = render(two);

    CHECK(oneStr.find("404") != std::string::npos);
    CHECK(oneStr.find("application/json") != std::string::npos);
    CHECK(oneStr.find("two") != std::string::npos);
    CHECK(oneStr.find("connection: close\r\n") == std::string::npos);

    CHECK(twoStr.find("200") != std::string::npos);
    CHECK(twoStr.find("text/plain") != std::string::npos);
    CHECK(twoStr.find("one") != std::string::npos);
    CHECK(twoStr.find("connection: close\r\n") != std::string::npos);
}
