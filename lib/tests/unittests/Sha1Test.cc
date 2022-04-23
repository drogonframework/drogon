#include "../../lib/src/ssl_funcs/Sha1.h"
#include <drogon/drogon_test.h>
#include <string>

DROGON_TEST(SHA1Test)
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
    CHECK(outStr == "fecfd28bbc9345891a66d7c1b8ff46e60192d284");
}