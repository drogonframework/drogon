#include <string_view>
#include <drogon/utils/Utilities.h>
#include <iostream>
#include <drogon/drogon_test.h>

DROGON_TEST(URLCodec)
{
    std::string input = "k1=1&k2=安";
    auto encoded = drogon::utils::urlEncode(input);
    auto decoded = drogon::utils::urlDecode(encoded);

    CHECK(encoded == "k1=1&k2=%E5%AE%89");
    CHECK(input == decoded);
}

// '+' stands for a space in form-urlencoded data (query strings and form
// bodies) only; in a path it is a literal character (RFC 3986, section 3.3).
DROGON_TEST(URLPathCodec)
{
    using namespace drogon::utils;
    const std::string encoded = "/a+b%20c%2Bd/%E5%AE%89";

    CHECK(urlDecode(encoded) == "/a b c+d/安");
    CHECK(urlDecodePath(encoded) == "/a+b c+d/安");
    CHECK(urlDecodePath(std::string_view{"+"}) == "+");
    // Malformed escapes are kept, as urlDecode() does.
    CHECK(urlDecodePath(std::string_view{"/a%2/b%zz+%"}) == "/a%2/b%zz+%");

    const std::string path = "/a+b c/安";
    CHECK(urlEncode(path) == "/a%2Bb+c/%E5%AE%89");
    CHECK(urlEncodePath(path) == "/a%2Bb%20c/%E5%AE%89");
    CHECK(urlDecodePath(urlEncodePath(path)) == path);
    // A form decoder reads both encodings of a space the same way.
    CHECK(urlDecode(urlEncodePath(path)) == path);
}
