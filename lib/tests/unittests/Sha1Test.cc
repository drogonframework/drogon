#include <drogon/utils/Utilities.h>
#include <drogon/drogon_test.h>
#include <string>

DROGON_TEST(SHA1Test)
{
    char in[] =
        "1234567890123456789012345678901234567890123456789012345"
        "678901234567890123456789012345678901234567890";
    auto str = drogon::utils::getSha1(in, strlen((const char *)in));
    CHECK(str == "FECFD28BBC9345891A66D7C1B8FF46E60192D284");
}