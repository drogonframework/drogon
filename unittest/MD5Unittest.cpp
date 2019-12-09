#include "../lib/src/ssl_funcs/Md5.h"
#include <gtest/gtest.h>
#include <string>

TEST(Md5Test, md5)
{
    // EXPECT_EQ(Md5Encode::encode("123456789012345678901234567890123456789012345"
    //                             "678901234567890123456789012345678901234567890"
    //                             "1234567890"),
    //           "49CB3608E2B33FAD6B65DF8CB8F49668");
    EXPECT_EQ(Md5Encode::encode("1"), "C4CA4238A0B923820DCC509A6F75849B");
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}