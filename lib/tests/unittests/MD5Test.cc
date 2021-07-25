#include <drogon/utils/Utilities.h>
#include <drogon/drogon_test.h>
#include <string>

DROGON_TEST(Md5Test)
{
    CHECK(drogon::utils::getMd5("123456789012345678901234567890123456789012345"
                                "678901234567890123456789012345678901234567890"
                                "1234567890") ==
          "49CB3608E2B33FAD6B65DF8CB8F49668");
    CHECK(drogon::utils::getMd5("1") == "C4CA4238A0B923820DCC509A6F75849B");
    CHECK(drogon::utils::getMd5("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                                "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                                "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF") ==
          "59F761506DFA597B0FAF1968F7CCA867");
}
