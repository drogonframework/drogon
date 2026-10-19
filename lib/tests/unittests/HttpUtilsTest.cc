#include "../lib/src/HttpUtils.h"
#include <drogon/drogon_test.h>
using namespace drogon;

DROGON_TEST(IsValidHeader)
{
    CHECK(isValidHeader("X-Test", "abc"));
    CHECK(isValidHeader("X-Test", ""));
    CHECK(isValidHeader("X-Test", "a: b; c=d"));
    CHECK(!isValidHeader("X-Test", "abc\r\nX-Injected: 1"));
    CHECK(!isValidHeader("X-Test", "abc\r"));
    CHECK(!isValidHeader("X-Test", "abc\nX-Injected: 1"));
    CHECK(!isValidHeader("X-Test\r\nX-Injected", "abc"));
    CHECK(!isValidHeader("X-Test\n", "abc"));
}
