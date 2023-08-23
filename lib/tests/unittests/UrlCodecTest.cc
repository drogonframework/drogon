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
