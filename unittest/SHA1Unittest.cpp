#include "../lib/src/ssl_funcs/Sha1.h"
#include <gtest/gtest.h>
#include <string>

TEST(SHA1Test, sha1)
{
    unsigned char in[] =
        "1234567890123456789012345678901234567890123456789012345"
        "678901234567890123456789012345678901234567890";
    unsigned char out[SHA_DIGEST_LENGTH] = {0};
    SHA1(in, strlen((const char *)in), out);
    std::string outStr;
    outStr.resize(SHA_DIGEST_LENGTH * 2);
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
        sprintf((char *)(outStr.data() + i * 2), "%02x", out[i]);
    EXPECT_EQ(outStr, "fecfd28bbc9345891a66d7c1b8ff46e60192d284");
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
